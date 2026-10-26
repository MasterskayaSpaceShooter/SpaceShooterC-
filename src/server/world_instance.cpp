#include "world_instance.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

#include "collision_utils.h"
#include "entity_utils.h"
#include "logger.h"
#include "network_response_router.h"

namespace {

/// Живые сущности: слот занят, не isDestroyed, есть transform и положительный радиус.
std::vector<const domain::Entity*> collectLiveEntities(const domain::EntityPool& pool) {
    std::vector<const domain::Entity*> result;
    for (const auto& slot : pool.storage()) {
        if (!pool.isActive(slot) || !slot->transform.has_value() || slot->radius <= 0.0) {
            continue;
        }
        result.push_back(&*slot);
    }
    return result;
}

}  // namespace

std::shared_ptr<WorldInstance> WorldInstance::create(boost::asio::io_context& io_context,
                                                     std::shared_ptr<events::EventBus> event_bus,
                                                     double tick_rate_hz,
                                                     math::Scalar world_width,
                                                     math::Scalar world_height) {
    auto world = std::make_shared<WorldInstance>(io_context,
                                                 std::move(event_bus),
                                                 tick_rate_hz,
                                                 CreateToken{},
                                                 world_width,
                                                 world_height);
    // в setupSubscriptions WI подписывается на события
    world->setupSubscriptions();
    return world;
}

WorldInstance::WorldInstance(boost::asio::io_context& io_context,
                             std::shared_ptr<events::EventBus> event_bus,
                             double tick_rate_hz,
                             CreateToken,
                             math::Scalar world_width,
                             math::Scalar world_height) :
    event_bus_(std::move(event_bus)), tick_strand_(boost::asio::make_strand(io_context)), tick_timer_(io_context),
    tick_interval_{}, world_width_(world_width), world_height_(world_height) {
    if (!event_bus_) {
        LOG_ERROR("[WorldInstance] event_bus is nullptr");
        throw std::invalid_argument("WorldInstance: event_bus must not be nullptr");
    }
    if (tick_rate_hz <= 0.0) {
        LOG_ERROR("[WorldInstance] invalid tick_rate_hz={}", tick_rate_hz);
        throw std::invalid_argument("WorldInstance: tick_rate_hz must be positive");
    }
    if (world_width_ <= 0.0 || world_height_ <= 0.0) {
        LOG_ERROR("[WorldInstance] invalid world_size: {}x{}", world_width_, world_height_);
        throw std::invalid_argument("WorldInstance: world_width and world_height must be positive");
    }

    tick_interval_ =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<double>(1.0 / tick_rate_hz));
    if (tick_interval_.count() < 1) {
        tick_interval_ = std::chrono::microseconds(1);
    }
}

WorldInstance::~WorldInstance() {
    // Постинг на strand невозможен в деструкторе (shared_from_this() недопустим).
    // Корректный жизненный цикл гарантирует: пока есть pending-операции тиков,
    // объект удерживается shared_from_this(), поэтому здесь операций не остаётся.
    // cancel() — защитный no-op.
    boost::system::error_code cancel_error;
    tick_timer_.cancel(cancel_error);
}

void WorldInstance::start() {
    boost::asio::post(tick_strand_, [self = shared_from_this()] {
        self->doStart();
    });
}

void WorldInstance::stop() {
    // Немедленно и детерминированно останавливаем каденцию тиков в вызывающем
    // потоке: это не даёт scheduleNextTick() перезавести таймер до того, как
    // doStop() доберётся до отмены. Если полагаться только на post(doStop),
    // возможна гонка: doTick() успевает перезавести async_wait (state ещё Running),
    // и io_context.run()/wait() никогда не завершается (зависание).
    state_.store(State::Stopped, std::memory_order_relaxed);
    boost::system::error_code cancel_error;
    tick_timer_.cancel(cancel_error);
    if (cancel_error) {
        LOG_ERROR("[WorldInstance] stop: timer cancel failed: {}", cancel_error.message());
    }

    // Очистку очереди и session bookkeeping выполняем на strand (как и раньше),
    // но остановка каденции выше уже состоялась и от неё не зависит.
    boost::asio::post(tick_strand_, [self = shared_from_this()] {
        self->doStop();
    });
}

void WorldInstance::doStart() {
    try {
        State expected{State::Stopped};
        if (!state_.compare_exchange_strong(expected, State::Running)) {
            return;
        }

        LOG_INFO("[WorldInstance] start: tick_interval={} ms",
                 std::chrono::duration<double, std::milli>(tick_interval_).count());

        // Начальная точка каденции: первый тик — через один интервал от now().
        tick_timer_.expires_after(tick_interval_);
        tick_timer_.async_wait(
            boost::asio::bind_executor(tick_strand_, [self = shared_from_this()](const boost::system::error_code& ec) {
                self->doTick(ec);
            }));
    } catch (const std::exception& e) {
        LOG_ERROR("[WorldInstance] start error: {}", e.what());
        state_.store(State::Stopped);
    } catch (...) {
        LOG_ERROR("[WorldInstance] start unknown error");
        state_.store(State::Stopped);
    }
}

void WorldInstance::doStop() {
    try {
        // state_ уже переведён в Stopped в stop() (синхронно). Здесь выполняем
        // только очистку, которая должна идти на strand: shutdownTickLoop()
        // отменяет таймер и дренирует очередь.
        LOG_INFO("[WorldInstance] stop");
        shutdownTickLoop();
    } catch (const std::exception& e) {
        LOG_ERROR("[WorldInstance] stop error: {}", e.what());
    } catch (...) {
        LOG_ERROR("[WorldInstance] stop unknown error");
    }
}

void WorldInstance::shutdownTickLoop() {
    // Отменяем таймер: активный async-хэндлер и заведённая каденция без обработки
    // тиков — это утёкший ресурс io_context, который помешает его run() завершиться.
    boost::system::error_code cancel_error;
    tick_timer_.cancel(cancel_error);
    if (cancel_error) {
        LOG_ERROR("[WorldInstance] stop: timer cancel failed: {}", cancel_error.message());
    }

    // Отбрасываем необработанные действия, накопленные в очереди. После stop()
    // тики больше не выполняются, поэтому такие команды не будут применены к миру;
    // оставлять их бессмысленно (устарели) и опасно (при перезапуске «оживут» и
    // применятся к новому состоянию игры). Молча терять их не стоит — логируем
    // количество, чтобы заметить, если клиенты продолжают спамить действиями.
    std::lock_guard action_lock(action_mutex_);
    const auto discarded = action_queue_.drain();
    if (!discarded.empty()) {
        LOG_WARN("[WorldInstance] stop: discarded {} queued actions", discarded.size());
    }
}

void WorldInstance::enqueueAction(ActionEvent action) {
    std::lock_guard lock(action_mutex_);
    if (state_.load() != State::Running) {
        LOG_WARN("[WorldInstance] discarded action because the world is stopped");
        return;
    }
    action_queue_.push(std::move(action));
}

void WorldInstance::doTick(const boost::system::error_code& error) {
    try {
        if (error) {
            if (error == boost::asio::error::operation_aborted) {
                return;
            }
            LOG_ERROR("[WorldInstance] tick timer error: {}", error.message());
            state_.store(State::Stopped);
            shutdownTickLoop();
            return;
        }

        if (state_.load() != State::Running) {
            return;
        }

        // Следующий тик планируется ДО выполнения тела: каденция привязана
        // к абсолютному дедлайну, а не к времени завершения тика.
        scheduleNextTick();

        runTickStep();
        ++current_tick_;
    } catch (const std::exception& e) {
        LOG_ERROR("[WorldInstance] tick #{} error: {}", current_tick_.load(), e.what());
        state_.store(State::Stopped);
        shutdownTickLoop();
    } catch (...) {
        LOG_ERROR("[WorldInstance] tick #{} unknown error", current_tick_.load());
        state_.store(State::Stopped);
        shutdownTickLoop();
    }
}

void WorldInstance::runTickStep() {
    try {
        // Новые игроки текущего тика: корабли создаются в spawnShips() после
        // расчёта физики, приветственный снимок им уходит в конце тика.
        std::vector<PendingJoin> new_players;
        processInputQueue(new_players);
        updatePhysics(static_cast<float>(tick_interval_.count()) / 1'000'000.0f);
        resolveCollisions();
        maintainAsteroidPopulation();
        // Спавн после физики: корабли новичков не участвуют в расчётах текущего тика.
        spawnShips(new_players);

        auto snapshot = buildGameSnapshot();
        // Приветственный снимок текущего тика (с кораблями новичков) доставляет
        // новому игроку broadcast: сессии регистрируются в SessionRegistry при
        // подключении (tcp_server.cpp), поэтому персональная рассылка по
        // new_players не требуется и лишь дублировала бы тот же payload.
        broadcastSnapshot(snapshot);
    } catch (const std::exception& e) {
        LOG_ERROR("[WorldInstance] tick #{} error: {}", current_tick_.load(), e.what());
    } catch (...) {
        LOG_ERROR("[WorldInstance] tick #{} unknown error", current_tick_.load());
    }
}

void WorldInstance::scheduleNextTick() {
    if (state_.load() != State::Running) {
        return;
    }

    // Жёсткая каденция: следующий дедлайн = предыдущий дедлайн + интервал.
    const auto next_deadline = tick_timer_.expiry() + tick_interval_;

    // Ограничение catch-up (защита от «спирали смерти»): если дедлайн
    // систематически отстаёт от реального времени (работа тика дольше
    // интервала), упущенные тики не догоняются скопом, а каденция
    // переустанавливается относительно now().
    if (next_deadline < std::chrono::steady_clock::now() - tick_interval_ * world::kMaxMissedTicks) {
        tick_timer_.expires_after(tick_interval_);
    } else {
        tick_timer_.expires_at(next_deadline);
    }

    tick_timer_.async_wait(
        boost::asio::bind_executor(tick_strand_, [self = shared_from_this()](const boost::system::error_code& ec) {
            self->doTick(ec);
        }));
}

std::uint64_t WorldInstance::currentTick() const noexcept {
    return current_tick_.load();
}

math::Circle WorldInstance::freePosition() const {
    const auto entities = collectLiveEntities(entities_);

    std::vector<std::pair<math::Vector2D, math::Scalar>> occupied;
    occupied.reserve(entities.size());
    for (const auto* e : entities) {
        occupied.emplace_back(e->transform->position, e->radius);
    }

    return math::largestEmptyCircle(occupied, world_width_, world_height_);
}

void WorldInstance::processInputQueue(std::vector<PendingJoin>& new_players) {
    const auto queued_actions = action_queue_.drain();
    for (const auto& action : queued_actions) {
        switch (action.type) {
            case ActionType::PlayerJoin: {
                new_players.push_back(PendingJoin{action.session_id, action.player_name});
                break;
            }
            case ActionType::PlayerQuit: {
                session_to_ship_.erase(action.session_id);
                break;
            }
            case ActionType::MoveAndShoot:
            case ActionType::Respawn:
            default:
                break;
        }
    }
}

void WorldInstance::updatePhysics(math::Scalar) {
    // TODO: обновить позиции кораблей, снарядов и астероидов за интервал тика (dt).
}

void WorldInstance::resolveCollisions() {
    // TODO: найти пересечения сущностей и вызвать соответствующие onCollision().
}

void WorldInstance::onCollision(domain::Entity& ship, domain::Entity& asteroid) {
    domain::collision::onCollision(ship, asteroid);
}

void WorldInstance::maintainAsteroidPopulation() {
    // TODO: поддерживать количество астероидов на карте через asteroid_factory_.
}

void WorldInstance::spawnShips(const std::vector<PendingJoin>& new_players) {
    if (new_players.empty()) {
        return;
    }

    // СПАВН В СВОБОДНОЙ ОБЛАСТИ:
    // Каждый корабль размещается в центре наибольшей свободной окружности поля
    // (freePosition()), не пересекаясь и не касаясь живых сущностей.
    //
    // Порядок критичен при спавне нескольких сущностей за тик:
    // freePosition() читает entities_ и считает «занятой» любую сущность с заполненным
    // transform и радиусом > 0. Поэтому freePosition() вызывается перед КАЖДЫМ acquire(),
    // а transform/radius нового корабля заполняются сразу после acquire() — так следующий
    // новичок получит заново пересчитанную «самую разряженную» область, а не ту же точку.
    for (const auto& join : new_players) {
        const math::Circle area = freePosition();  // <— до acquire()
        if (area.radius < world::kShipDefaultRadius) {
            LOG_WARN("[WorldInstance] spawn: crowded world, free radius {:.3f} < ship radius {:.3f}",
                     area.radius,
                     world::kShipDefaultRadius);
        }

        auto* entity = entities_.acquire();
        entity->type = types::EntityType::Ship;
        entity->mass = world::kShipDefaultMass;
        entity->radius = world::kShipDefaultRadius;
        // transform/radius заполняются сразу, чтобы freePosition() на следующей
        // итерации учитывал этот корабль как занятый.
        entity->transform = domain::TransformComponent{area.center, {0.0, 0.0}, 0.0};

        domain::ShipComponent ship;
        ship.name.fill('\0');
        const std::size_t len = std::min(join.player_name.size(), ship.name.size() - 1);
        std::copy_n(join.player_name.data(), len, ship.name.begin());
        ship.thrust_power = world::kShipDefaultThrustPower;
        ship.drag = world::kShipDefaultDrag;
        entity->ship = ship;

        entity->health = domain::HealthComponent{world::kShipDefaultHealth, world::kShipDefaultHealth};
        entity->combat = domain::CombatComponent{world::kShipDefaultDamage, entity->id};

        // Привязываем сессию к кораблю: по ней ищется корабль при обработке
        // команд игрока и удалении при PlayerQuit.
        session_to_ship_[join.session_id] = entity->id;
    }
}

snapshot::GameSnapshot WorldInstance::buildGameSnapshot() {
    domain::buildEntitySnapshots(entities_, snapshot_buffer_);
    const auto tick_number = static_cast<std::uint32_t>(current_tick_.load());
    // Копируем, чтобы переиспользовать буфер на следующем тике (ёмкость сохраняется).
    return snapshot::GameSnapshot{snapshot_buffer_, tick_number};
}

void WorldInstance::broadcastSnapshot(const snapshot::GameSnapshot& snapshot) {
    // Сериализуем готовый снимок мира и публикуем в шину событий.
    // session_id == 0 (kBroadcastSessionId) — маркер массовой рассылки всем клиентам,
    // обработку берёт на себя SessionRegistry.
    const auto payload = snapshot.Serialize();
    event_bus_->publish(SendPacketEvent{NetworkResponseRouter::kBroadcastSessionId, std::move(payload)});
}

void WorldInstance::setupSubscriptions() {
    // weak_from_this нужна чтобы проверить, жив ли WI, когда сработает колбэк
    auto weak_self = weak_from_this();

    // подписка на отключение клиента
    sub_disconnect_ = event_bus_->subscribe<ClientDisconnectedEvent>([weak_self](const ClientDisconnectedEvent& event) {
        if (auto world = weak_self.lock()) {
            world->onClientDisconnected(event);
        }
    });

    // далее другие подписки
}

void WorldInstance::onClientDisconnected(const ClientDisconnectedEvent& event) {
    ActionEvent action;
    action.type = ActionType::PlayerQuit;
    action.session_id = event.session_id;
    enqueueAction(std::move(action));
    LOG_INFO("[WorldInstance] PlayerQuit for session {}", event.session_id);
}

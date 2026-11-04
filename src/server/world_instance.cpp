#include "world_instance.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

#include "collision_utils.h"
#include "entity_utils.h"
#include "logger.h"
#include "network_response_router.h"

namespace {

/// Живые сущности: слот занят, не isDestroyed, есть transform и положительный радиус.
inline std::vector<const domain::Entity*> collectLiveEntities(const domain::EntityPool& pool) {
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
    entities_{}, event_bus_(std::move(event_bus)), tick_strand_(boost::asio::make_strand(io_context)),
    tick_timer_(io_context), tick_interval_{}, world_width_(world_width), world_height_(world_height),
    asteroid_factory_(entities_) {
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
                auto it = std::find_if(new_players.begin(), new_players.end(), [&action](const PendingJoin& p) {
                    return p.session_id == action.session_id;
                });

                if (it != new_players.end()) {
                    // пропускаем игрока, если тот уже есть в new_players
                    LOG_WARN("[WorldInstance] Duplicate PlayerJoin for session {}, ignoring", action.session_id);
                    break;
                }
                new_players.push_back(PendingJoin{action.session_id, action.player_name});
                break;
            }
            case ActionType::PlayerQuit: {
                auto it = session_to_ship_.find(action.session_id);
                if (it == session_to_ship_.end())
                    break;

                const types::EntityId ship_id = it->second;
                if (domain::Entity* ship = entities_.findByEntityID(ship_id)) {
                    ship->isDestroyed = true;
                }
                session_to_ship_.erase(it);
                // No ShipDestroyedEvent — the player is gone, this is not a kill.
                break;
            }
            case ActionType::MoveAndShoot: {
                /* Достаём EntityId из мапы session_to_ship_ по action.session_id. Если игрок стреляет, то берём данные
                 * корабля из EntityPool, создаём пулю и задаём ей параметры направления. */
                auto it = session_to_ship_.find(action.session_id);
                if (it != session_to_ship_.end()) {
                    auto ship_id = it->second;

                    if (domain::Entity* ship = entities_.findByEntityID(ship_id)) {
                        if (ship->transform.has_value()) {
                            ship->transform->angle = action.angle;

                            if (action.is_shooting) {
                                auto bullet = entities_.acquire();
                                bullet->type = types::EntityType::Bullet;
                                bullet->radius = world::kBulletRadius;
                                bullet->transform = domain::TransformComponent{};
                                bullet->combat = domain::CombatComponent{};

                                auto angle = ship->transform->angle;
                                math::Vector2D direction{std::cos(angle), std::sin(angle)};
                                bullet->transform->position = ship->transform->position + (direction * ship->radius);
                                bullet->transform->velocity = direction * world::kBulletSpeed;

                                bullet->combat->owner_id = ship_id;
                                bullet->combat->damage = world::kBulletDamage;
                            }
                        }
                    }
                }
                break;
            }
            case ActionType::Respawn:
            default:
                break;
        }
    }
}

void WorldInstance::updatePhysics(math::Scalar dt) {
    domain::updateMovementAndBounds(entities_.storage(), dt, world_width_, world_height_);
}

void WorldInstance::resolveCollisions() {
    auto& storage = entities_.storage();
    const std::size_t n = storage.size();

    for (std::size_t i = 0; i < n; ++i) {
        auto& a_opt = storage[i];
        if (!a_opt || a_opt->isDestroyed || !a_opt->transform)
            continue;

        for (std::size_t j = i + 1; j < n; ++j) {
            if (a_opt->isDestroyed)  // сущность уничтожена в dispatchCollision — дальнейшие пары пропускаем
                break;
            auto& b_opt = storage[j];
            if (!b_opt || b_opt->isDestroyed || !b_opt->transform)
                continue;
            if (!mayCollide(a_opt->type, b_opt->type))
                continue;
            if (!math::collision::checkCircleCollision(a_opt->transform->position,
                                                       a_opt->radius,
                                                       b_opt->transform->position,
                                                       b_opt->radius)) {
                continue;
            }
            dispatchCollision(*a_opt, *b_opt);
        }
    }

    reapDestroyedShips();
}

bool WorldInstance::mayCollide(types::EntityType a, types::EntityType b) const noexcept {
    using T = types::EntityType;
    if (a == T::None || b == T::None)
        return false;
    if (a == T::Bullet && b == T::Bullet)
        return false;
    if (a == T::Asteroid && b == T::Asteroid)
        return false;
    return true;
}

void WorldInstance::dispatchCollision(domain::Entity& a, domain::Entity& b) {
    using T = types::EntityType;
    const T ta = a.type;
    const T tb = b.type;

    if (ta == T::Ship && tb == T::Ship)
        onCollisionShipShip(a, b);
    else if (ta == T::Ship && tb == T::Asteroid)
        onCollisionShipAsteroid(a, b);
    else if (ta == T::Asteroid && tb == T::Ship)
        onCollisionShipAsteroid(b, a);
    else if (ta == T::Bullet && tb == T::Ship)
        onCollisionBulletShip(a, b);
    else if (ta == T::Ship && tb == T::Bullet)
        onCollisionBulletShip(b, a);
    else if (ta == T::Bullet && tb == T::Asteroid)
        onCollisionBulletAsteroid(a, b);
    else if (ta == T::Asteroid && tb == T::Bullet)
        onCollisionBulletAsteroid(b, a);
}

void WorldInstance::onCollisionBulletAsteroid(domain::Entity& proj, domain::Entity& asteroid) {
    // TODO: определить семантику столкновения пули с астероидом: пуля разрушается об астероид
    // (proj.isDestroyed = true), астероид получает урон или делится на осколки. Пока коллизия
    // игнорируется — пуля продолжает лететь сквозь астероид.
    (void)proj;
    (void)asteroid;
}

void WorldInstance::reapDestroyedShips() {
    // Один проход по пулу: собираем id всех сущностей, помеченных isDestroyed.
    std::unordered_set<types::EntityId, types::EntityHash> destroyed_ids;
    for (const auto& opt : entities_.storage()) {
        if (opt && opt->isDestroyed) {
            destroyed_ids.insert(opt->id);
        }
    }

    // Publish one ShipDestroyedEvent per session whose ship died this tick,
    // then drop the mapping, then recycle the pool.
    // PlayerQuit path already removed its mapping in processInputQueue(),
    // so those ships are not revisited here (idempotent).
    for (auto it = session_to_ship_.begin(); it != session_to_ship_.end();) {
        if (destroyed_ids.contains(it->second)) {
            LOG_INFO("[WorldInstance] ship destroyed: session={}, entity={}", it->first, it->second.get());
            event_bus_->publish(ShipDestroyedEvent{it->first});
            it = session_to_ship_.erase(it);
        } else {
            ++it;
        }
    }

    // Инцидент: уничтоженный корабль без привязки к сессии — событие не публикуем, только лог.
    for (const auto& opt : entities_.storage()) {
        if (!opt || !opt->isDestroyed || opt->type != types::EntityType::Ship)
            continue;

        bool has_session = false;
        for (const auto& [sid, eid] : session_to_ship_) {
            if (eid == opt->id) {
                has_session = true;
                break;
            }
        }
        if (!has_session) {
            LOG_WARN("[WorldInstance] ship {} destroyed without session binding", opt->id.get());
        }
    }

    entities_.recycleDestroyed();
}

void WorldInstance::onCollisionShipShip(domain::Entity& ship1, domain::Entity& ship2) {
    domain::collision::onCollisionShipShip(ship1, ship2);
}

WorldInstance::GrowStatus WorldInstance::tryGrowShip(math::Vector2D& position, math::Scalar radius) const noexcept {
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(radius) || radius < 0.0 ||
        !std::isfinite(world_width_) || !std::isfinite(world_height_) || world_width_ <= 0.0 || world_height_ <= 0.0) {
        LOG_ERROR("[WorldInstance] cannot grow ship with invalid position, radius, or world dimensions");
        return GrowStatus::InvalidInput;
    }

    position = math::clampToWorldBounds(position, world_width_, world_height_);
    if (math::circleIntersectsOppositeWorldBounds(position, radius, world_width_, world_height_)) {
        return GrowStatus::CannotFit;
    }
    return GrowStatus::CanGrow;
}

void WorldInstance::onCollisionShipAsteroid(domain::Entity& ship, domain::Entity& asteroid) {
    auto grow_status = GrowStatus::CanGrow;
    const auto can_grow = [this, &grow_status](math::Vector2D& position, math::Scalar radius) noexcept -> bool {
        grow_status = tryGrowShip(position, radius);
        return grow_status == GrowStatus::CanGrow;
    };
    domain::collision::onCollisionShipAsteroid(ship, asteroid, can_grow);

    if (grow_status == GrowStatus::CannotFit) {
        ship.isDestroyed = true;
        LOG_INFO("[WorldInstance] ship {} does not fit the world, destroyed", ship.id.get());
    }
}

void WorldInstance::onCollisionBulletShip(domain::Entity& proj, domain::Entity& ship) {
    domain::collision::onCollisionBulletShip(proj, ship, [this](types::EntityId owner_id, bool ship_destroyed) {
        for (auto& slot : entities_.storage()) {
            if (!slot || slot->isDestroyed || slot->id != owner_id) {
                continue;
            }

            if (slot->type != types::EntityType::Ship || !slot->ship) {
                LOG_WARN("[Collision] Bullet owner {} is not a valid ship", owner_id.get());
                return;
            }

            const std::uint32_t score = ship_destroyed ? 50 : 5;
            domain::addScore(*slot->ship, score);

            LOG_INFO("[Collision] Ship {} received {} points (target destroyed: {})",
                     owner_id.get(),
                     score,
                     ship_destroyed);
            return;
        }

        LOG_WARN("[Collision] Bullet owner ship {} not found", owner_id.get());
    });
}

void WorldInstance::maintainAsteroidPopulation() {
    asteroid_factory_.update(freePosition());
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
        ship.is_thrusting = true;
        entity->ship = ship;

        entity->health = domain::HealthComponent{world::kShipDefaultHealth, world::kShipDefaultHealth};
        entity->combat = domain::CombatComponent{world::kShipDefaultDamage, entity->id};

        // Привязываем сессию к кораблю: по ней ищется корабль при обработке
        // команд игрока и удалении при PlayerQuit.
        session_to_ship_[join.session_id] = entity->id;
    }
}

std::vector<snapshot::LeaderboardEntry> WorldInstance::buildLeaderboard() const {
    std::vector<snapshot::LeaderboardEntry> top_players;

    for (const auto& slot : entities_.storage()) {
        // Пропускаем пустые слоты
        if (!slot.has_value())
            continue;
        const auto& entity = slot.value();

        // Учитываем только живых игроков (тип Ship + есть компонент + health > 0)
        if (entity.type == types::EntityType::Ship && entity.ship.has_value() && entity.health.has_value() &&
            entity.health->current > 0 && !entity.isDestroyed) {
            // Конвертируем char[32] в std::string
            std::string_view view(entity.ship->name.data(), entity.ship->name.size());
            if (auto idx = view.find('\0'); idx != std::string_view::npos) {
                view = view.substr(0, idx);
            }
            top_players.push_back({std::string(view), entity.ship->score});
        }
    }

    const size_t top_count = std::min(world::kMaxLeaderboardSize, top_players.size());

    std::partial_sort(top_players.begin(),
                      top_players.begin() + top_count,
                      top_players.end(),
                      [](const auto& a, const auto& b) {
                          return a.score > b.score;
                      });

    top_players.resize(top_count);
    return top_players;
}

snapshot::GameSnapshot WorldInstance::buildGameSnapshot() {
    // Собираем снимки сущностей (логика из main)
    domain::buildEntitySnapshots(entities_, snapshot_buffer_);

    // Собираем таблицу лидеров
    auto leaderboard = buildLeaderboard();

    // Возвращаем GameSnapshot с тремя аргументами
    const auto tick_number = static_cast<std::uint32_t>(current_tick_.load());
    return snapshot::GameSnapshot{snapshot_buffer_, std::move(leaderboard), tick_number};
}

void WorldInstance::broadcastSnapshot(const snapshot::GameSnapshot& snapshot) {
    // Сериализуем готовый снимок мира и публикуем в шину событий.
    // session_id == 0 (kBroadcastSessionId) — маркер массовой рассылки всем клиентам,
    // обработку берёт на себя SessionRegistry.
    auto payload = snapshot.Serialize();
    event_bus_->publish(SendPacketEvent{NetworkResponseRouter::kBroadcastSessionId, std::move(payload)});

    // удаляем мёртвых из памяти пула
    entities_.recycleDestroyed();
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

    // подписка на вхождение клиента в игру
    sub_player_join_ = event_bus_->subscribe<PlayerJoinEvent>([weak_self](const PlayerJoinEvent& event) {
        if (auto world = weak_self.lock()) {
            world->onPlayerJoin(event);
        }
    });

    sub_shoot_ = event_bus_->subscribe<ShotMessageEvent>([weak_self](const ShotMessageEvent& event) {
        if (auto world = weak_self.lock()) {
            world->onShotReceived(event);
        }
    });

    // подписка на движения клиента
    sub_player_move_ = event_bus_->subscribe<PlayerMoveEvent>([weak_self](const PlayerMoveEvent& event) {
        if (auto world = weak_self.lock()) {
            world->onPlayerMove(event);
        }
    });
}

void WorldInstance::onClientDisconnected(const ClientDisconnectedEvent& event) {
    ActionEvent action;
    action.type = ActionType::PlayerQuit;
    action.session_id = event.session_id;
    enqueueAction(std::move(action));
    LOG_INFO("[WorldInstance] PlayerQuit for session {}", event.session_id);
}

void WorldInstance::onPlayerJoin(const PlayerJoinEvent& event) {
    ActionEvent action;
    action.type = ActionType::PlayerJoin;
    action.session_id = event.session_id;
    action.player_name = event.login;
    enqueueAction(std::move(action));
    LOG_INFO("[WorldInstance] PlayerJoin: {} join to game from session {}", event.login, event.session_id);
}

void WorldInstance::onShotReceived(const ShotMessageEvent& event) {
    ActionEvent action;
    action.type = ActionType::MoveAndShoot;
    action.session_id = event.session_id;
    action.is_shooting = true;
    enqueueAction(std::move(action));
    LOG_INFO("[WorldInstance] MoveAndShoot for session {}", event.session_id);
}

void WorldInstance::onPlayerMove(const PlayerMoveEvent& event) {
    ActionEvent action;
    action.type = ActionType::MoveAndShoot;
    action.session_id = event.session_id;
    action.angle = event.angle;
    enqueueAction(std::move(action));
    LOG_INFO("[WorldInstance] PlayerMove: {}: move on angle {}", event.session_id, event.angle);
}

#include "world_instance.h"

#include <exception>
#include <stdexcept>
#include <utility>

#include "logger.h"

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
        State expected{State::Running};
        if (!state_.compare_exchange_strong(expected, State::Stopped)) {
            return;
        }

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
    const auto discarded = action_queue_.drain();
    if (!discarded.empty()) {
        LOG_WARN("[WorldInstance] stop: discarded {} queued actions", discarded.size());
    }
}

void WorldInstance::enqueueAction(ActionEvent action) {
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
        processInputQueue();
        updatePhysics(static_cast<float>(tick_interval_.count()) / 1'000'000.0f);
        resolveCollisions();
        maintainAsteroidPopulation();

        {
            std::lock_guard lock(last_snapshot_mutex_);
            last_snapshot_ = std::make_shared<snapshot::GameSnapshot>(buildGameSnapshot());
        }
        broadcastSnapshot();
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

std::shared_ptr<const snapshot::GameSnapshot> WorldInstance::lastSnapshot() const noexcept {
    std::lock_guard lock(last_snapshot_mutex_);
    return last_snapshot_;
}

void WorldInstance::processInputQueue() {
    // TODO: извлечь действия игроков из action_queue_ и применить их к миру.
}

void WorldInstance::updatePhysics(float) {
    // TODO: обновить позиции кораблей, снарядов и астероидов за интервал тика (dt).
}

void WorldInstance::resolveCollisions() {
    // TODO: найти пересечения сущностей и вызвать соответствующие onCollision().
}

void WorldInstance::maintainAsteroidPopulation() {
    // TODO: поддерживать количество астероидов на карте через asteroid_factory_.
}

snapshot::GameSnapshot WorldInstance::buildGameSnapshot() {
    // TODO: собрать снимок состояния мира для рассылки клиентам.
    return snapshot::GameSnapshot{};
}

void WorldInstance::broadcastSnapshot() {
    // TODO: разослать buildGameSnapshot() всем клиентам через event_bus_.
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

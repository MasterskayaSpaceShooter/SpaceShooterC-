#include "world_instance.h"

#include <exception>
#include <stdexcept>
#include <utility>

#include "logger.h"

std::shared_ptr<WorldInstance> WorldInstance::create(boost::asio::io_context& io_context,
                                                     std::shared_ptr<events::EventBus> event_bus,
                                                     double tick_rate_hz) {
    return std::shared_ptr<WorldInstance>(new WorldInstance(io_context, std::move(event_bus), tick_rate_hz));
}

WorldInstance::WorldInstance(boost::asio::io_context& io_context,
                             std::shared_ptr<events::EventBus> event_bus,
                             double tick_rate_hz) :
    event_bus_(std::move(event_bus)), tick_timer_(io_context), tick_interval_{} {
    if (tick_rate_hz <= 0.0) {
        LOG_ERROR("[WorldInstance] invalid tick_rate_hz={}", tick_rate_hz);
        throw std::invalid_argument("WorldInstance: tick_rate_hz must be positive");
    }

    tick_interval_ = std::chrono::milliseconds(static_cast<std::int64_t>(1000.0 / tick_rate_hz));
    if (tick_interval_.count() < 1) {
        tick_interval_ = std::chrono::milliseconds(1);
    }
}

WorldInstance::~WorldInstance() {
    stop();
}

void WorldInstance::start() {
    if (is_running_.exchange(true)) {
        return;
    }

    LOG_INFO("[WorldInstance] start: tick_interval={} ms", tick_interval_.count());

    doTick({});
}

void WorldInstance::stop() {
    if (!is_running_.exchange(false)) {
        return;
    }

    boost::system::error_code cancel_error;
    tick_timer_.cancel(cancel_error);
    if (cancel_error) {
        LOG_ERROR("[WorldInstance] stop: timer cancel failed: {}", cancel_error.message());
    } else {
        LOG_INFO("[WorldInstance] stop");
    }
}

void WorldInstance::enqueueAction(ActionEvent action) {
    action_queue_.push(std::move(action));
}

void WorldInstance::doTick(const boost::system::error_code& error) {
    if (error) {
        if (error == boost::asio::error::operation_aborted) {
            return;
        }
        LOG_ERROR("[WorldInstance] tick timer error: {}", error.message());
        is_running_.store(false);
        return;
    }

    if (!is_running_.load()) {
        return;
    }

    try {
        processInputQueue();
        updatePhysics(static_cast<float>(tick_interval_.count()) / 1000.0f);
        resolveCollisions();
        maintainAsteroidPopulation();

        {
            std::lock_guard lock(last_snapshot_mutex_);
            last_snapshot_ = std::make_shared<GameSnapshot>(buildGameSnapshot());
        }
        broadcastSnapshot();
    } catch (const std::exception& e) {
        LOG_ERROR("[WorldInstance] tick #{} error: {}", current_tick_.load(), e.what());
    } catch (...) {
        LOG_ERROR("[WorldInstance] tick #{} unknown error", current_tick_.load());
    }

    ++current_tick_;

    tick_timer_.expires_after(tick_interval_);
    tick_timer_.async_wait([self = shared_from_this()](const boost::system::error_code& ec) {
        self->doTick(ec);
    });
}

std::uint64_t WorldInstance::currentTick() const noexcept {
    return current_tick_.load();
}

std::shared_ptr<const GameSnapshot> WorldInstance::lastSnapshot() const noexcept {
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

GameSnapshot WorldInstance::buildGameSnapshot() {
    // TODO: собрать снимок состояния мира для рассылки клиентам.
    return GameSnapshot{};
}

void WorldInstance::broadcastSnapshot() {
    // TODO: разослать buildGameSnapshot() всем клиентам через event_bus_.
}

#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

#include "network_context.h"
#include "world_instance.h"

/// Вспомогательный класс для доступа к приватным полям WI в тестах.
class WorldInstanceTestAccessor {
public:
    // Возвращает накопленные действия из action_queue_
    static std::vector<ActionEvent> drainActions(WorldInstance& world) {
        return world.action_queue_.drain();
    }

    // Возвращает мапу session_to_ship_
    static auto GetSessionToShip(const WorldInstance& world) {
        return world.session_to_ship_;
    }

    // Возвращает кол-во слотов помеченных мервыми
    static auto GetEntityCountWithStatusAlive(const WorldInstance& world) {
        size_t count = 0;
        for (const auto& slot : world.entities_.storage()) {
            if (slot.has_value() && !slot->isDestroyed) {
                count++;
            }
        }
        return count;
    }
};

TEST(WorldInstance, CreateWithNullEventBusThrows) {
    boost::asio::io_context io_context;

    EXPECT_THROW(WorldInstance::create(io_context, nullptr), std::invalid_argument);
}

TEST(WorldInstance, RejectsNonPositiveTickRate) {
    boost::asio::io_context io_context;

    EXPECT_THROW(WorldInstance::create(io_context, events::EventBus::create(), 0.0), std::invalid_argument);
}

TEST(WorldInstance, LastSnapshotIsNullBeforeFirstTick) {
    NetworkContext nc(1);
    auto world = WorldInstance::create(nc.getContext(), events::EventBus::create());

    EXPECT_EQ(world->lastSnapshot(), nullptr);
    world->stop();
}

TEST(WorldInstance, FreePositionsReturnsCandidatesInsideWorld) {
    boost::asio::io_context io_context;
    auto world = WorldInstance::create(io_context, events::EventBus::create(), 60.0, 100.0, 100.0);

    const auto positions = world->freePositions(3, 10.0);

    ASSERT_EQ(positions.size(), 3u);
    for (const auto& position : positions) {
        EXPECT_GE(position.x, -40.0);
        EXPECT_LE(position.x, 40.0);
        EXPECT_GE(position.y, -40.0);
        EXPECT_LE(position.y, 40.0);
    }
}

TEST(WorldInstance, StopWithoutStartIsNoOp) {
    boost::asio::io_context io_context;
    auto world = WorldInstance::create(io_context, events::EventBus::create());

    EXPECT_NO_THROW(world->stop());
}

TEST(WorldInstance, GameLoopTicksAfterStart) {
    NetworkContext nc(1);
    auto world = WorldInstance::create(nc.getContext(), events::EventBus::create(), 100.0);

    world->start();
    nc.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    EXPECT_GT(world->currentTick(), 0u);

    world->stop();
    nc.stop();
    nc.wait();
}

TEST(WorldInstance, EnqueueActionsConcurrentlyWhileRunning) {
    constexpr size_t kThreads = 4;
    constexpr size_t kActionsPerThread = 1000;

    NetworkContext nc(1);
    auto world = WorldInstance::create(nc.getContext(), events::EventBus::create(), 100.0);
    world->start();
    nc.start();

    std::vector<std::thread> writers;
    for (size_t i = 0; i < kThreads; ++i) {
        writers.emplace_back([&world] {
            for (size_t j = 0; j < kActionsPerThread; ++j) {
                world->enqueueAction(ActionEvent{});
            }
        });
    }
    for (auto& thread : writers) {
        thread.join();
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (world->currentTick() == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_GT(world->currentTick(), 0u);

    world->stop();
    nc.stop();
    nc.wait();
}

TEST(WorldInstance, StopWaitsForTickLoopToStop) {
    NetworkContext nc(1);
    auto world = WorldInstance::create(nc.getContext(), events::EventBus::create(), 100.0);
    world->start();
    nc.start();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (world->currentTick() == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_GT(world->currentTick(), 0u);

    world->stop();
    const auto stopped_tick = world->currentTick();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_EQ(world->currentTick(), stopped_tick);

    nc.stop();
    nc.wait();
}

// Проверка получения события PlayerJoinEvent в очередь действий
TEST(WorldInstance, ReceivedPlayerJoin) {
    NetworkContext nc(1);
    auto event_bus = events::EventBus::create();
    auto world = WorldInstance::create(nc.getContext(), event_bus, 100.0);
    world->start();
    nc.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    event_bus->publish(PlayerJoinEvent{42, "TestName"});

    // ждём пока заполниться мапа
    const auto map_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (WorldInstanceTestAccessor::GetSessionToShip(*world).size() == 0 &&
           std::chrono::steady_clock::now() < map_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    auto session_to_ship = WorldInstanceTestAccessor::GetSessionToShip(*world);
    EXPECT_EQ(session_to_ship.size(), 1);

    world->stop();
    nc.force_stop();
    nc.wait();
}

// Проверка получения события ClientDisconnectedEvent в очередь действий
TEST(WorldInstance, ReceivedPlayerQuit) {
    NetworkContext nc(1);
    auto event_bus = events::EventBus::create();
    auto world = WorldInstance::create(nc.getContext(), event_bus, 100.0);
    nc.start();
    world->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    event_bus->publish(PlayerJoinEvent{42, "TestName"});
    // ждём пока заполниться мапа
    const auto map_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (WorldInstanceTestAccessor::GetSessionToShip(*world).size() == 0 &&
           std::chrono::steady_clock::now() < map_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Проверяем, что корабль и связь есть
    EXPECT_EQ(WorldInstanceTestAccessor::GetSessionToShip(*world).size(), 1);

    event_bus->publish(ClientDisconnectedEvent{42});
    const auto delete_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (WorldInstanceTestAccessor::GetSessionToShip(*world).size() > 0 &&
           std::chrono::steady_clock::now() < delete_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Проверяем, что корабля и связи нет
    EXPECT_EQ(WorldInstanceTestAccessor::GetSessionToShip(*world).size(), 0);

    world->stop();
    nc.force_stop();
    nc.wait();
}

// Проверка пула сущностей при PlayerJoin
TEST(WorldInstance, StatusEntityPoolPlayerJoin) {
    NetworkContext nc(1);
    auto event_bus = events::EventBus::create();
    auto world = WorldInstance::create(nc.getContext(), event_bus, 100.0);
    nc.start();
    world->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    event_bus->publish(PlayerJoinEvent{42, "TestName"});
    const auto pool_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (WorldInstanceTestAccessor::GetEntityCountWithStatusAlive(*world) == 0 &&
           std::chrono::steady_clock::now() < pool_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Проверяем, что корабль в пуле сущностей
    EXPECT_EQ(WorldInstanceTestAccessor::GetEntityCountWithStatusAlive(*world), 1);

    world->stop();
    nc.force_stop();
    nc.wait();
}

// Проверка пула сущностей при ClientDisconnectedEvent
TEST(WorldInstance, StatusEntityPoolPlayerQuit) {
    NetworkContext nc(1);
    auto event_bus = events::EventBus::create();
    auto world = WorldInstance::create(nc.getContext(), event_bus, 100.0);
    nc.start();
    world->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // подлючение клиента и отключение
    event_bus->publish(PlayerJoinEvent{42, "TestName"});
    const auto pool_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (WorldInstanceTestAccessor::GetEntityCountWithStatusAlive(*world) == 0 &&
           std::chrono::steady_clock::now() < pool_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    event_bus->publish(ClientDisconnectedEvent{42});
    const auto delete_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (WorldInstanceTestAccessor::GetEntityCountWithStatusAlive(*world) > 0 &&
           std::chrono::steady_clock::now() < delete_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    // Проверяем, что корабль в пуле сущностей
    EXPECT_EQ(WorldInstanceTestAccessor::GetEntityCountWithStatusAlive(*world), 0);

    world->stop();
    nc.force_stop();
    nc.wait();
}

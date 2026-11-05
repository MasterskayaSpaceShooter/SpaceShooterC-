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
    static domain::Entity& acquireEntity(WorldInstance& world) {
        return *world.entities_.acquire();
    }

    static void collideShipAsteroid(WorldInstance& world, domain::Entity& ship, domain::Entity& asteroid) {
        world.onCollisionShipAsteroid(ship, asteroid);
    }

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
            if (slot.has_value() && !slot->isDestroyed && slot->type == types::EntityType::Ship) {
                count++;
            }
        }
        return count;
    }
};

TEST(WorldInstance, GrowingShipResolvesCollisionWithNearbyShip) {
    boost::asio::io_context io_context;
    auto world = WorldInstance::create(io_context, events::EventBus::create(), 60.0, 1000.0, 1000.0);

    auto& ship = WorldInstanceTestAccessor::acquireEntity(*world);
    ship.type = types::EntityType::Ship;
    ship.mass = 300.0;
    ship.radius = 5.0;
    ship.transform = domain::TransformComponent{};
    ship.health = domain::HealthComponent{100, 100};
    ship.ship = domain::ShipComponent{};

    auto& neighbor = WorldInstanceTestAccessor::acquireEntity(*world);
    neighbor.type = types::EntityType::Ship;
    neighbor.mass = 200.0;
    neighbor.radius = 5.0;
    neighbor.transform = domain::TransformComponent{{14.0, 0.0}, {}, 0.0};
    neighbor.health = domain::HealthComponent{100, 100};
    neighbor.ship = domain::ShipComponent{};

    auto& asteroid = WorldInstanceTestAccessor::acquireEntity(*world);
    asteroid.type = types::EntityType::Asteroid;
    asteroid.mass = 100.0;
    asteroid.radius = 2.0;
    asteroid.transform = domain::TransformComponent{};
    asteroid.health = domain::HealthComponent{100, 100};
    asteroid.asteroid = domain::AsteroidComponent{};

    WorldInstanceTestAccessor::collideShipAsteroid(*world, ship, asteroid);

    EXPECT_EQ(ship.radius, 10.0);
    EXPECT_EQ(ship.health->current, 95);
    EXPECT_EQ(neighbor.health->current, 90);
    EXPECT_TRUE(asteroid.isDestroyed);
}

TEST(WorldInstance, EachGrowthStepResolvesTheShipCollisionAgain) {
    boost::asio::io_context io_context;
    auto world = WorldInstance::create(io_context, events::EventBus::create(), 60.0, 1000.0, 1000.0);

    auto& ship = WorldInstanceTestAccessor::acquireEntity(*world);
    ship.type = types::EntityType::Ship;
    ship.mass = 2000.0;
    ship.radius = 5.0;
    ship.transform = domain::TransformComponent{};
    ship.health = domain::HealthComponent{100, 100};
    ship.ship = domain::ShipComponent{};

    auto& neighbor = WorldInstanceTestAccessor::acquireEntity(*world);
    neighbor.type = types::EntityType::Ship;
    neighbor.mass = 1000.0;
    neighbor.radius = 5.0;
    neighbor.transform = domain::TransformComponent{{15.0, 0.0}, {}, 0.0};
    neighbor.health = domain::HealthComponent{100, 100};
    neighbor.ship = domain::ShipComponent{};

    auto& asteroid = WorldInstanceTestAccessor::acquireEntity(*world);
    asteroid.type = types::EntityType::Asteroid;
    asteroid.mass = 900.0;
    asteroid.radius = 2.0;
    asteroid.transform = domain::TransformComponent{};
    asteroid.health = domain::HealthComponent{100, 100};
    asteroid.asteroid = domain::AsteroidComponent{};

    WorldInstanceTestAccessor::collideShipAsteroid(*world, ship, asteroid);

    EXPECT_EQ(ship.radius, 20.0);
    EXPECT_EQ(ship.health->current, 90);
    EXPECT_EQ(neighbor.health->current, 80);
}

TEST(WorldInstance, ShipGrowingIntoEnemyBulletTakesDamageAndDestroysBullet) {
    boost::asio::io_context io_context;
    auto world = WorldInstance::create(io_context, events::EventBus::create(), 60.0, 1000.0, 1000.0);

    auto& ship = WorldInstanceTestAccessor::acquireEntity(*world);
    ship.type = types::EntityType::Ship;
    ship.mass = 300.0;
    ship.radius = 5.0;
    ship.transform = domain::TransformComponent{};
    ship.health = domain::HealthComponent{100, 100};
    ship.ship = domain::ShipComponent{};

    auto& asteroid = WorldInstanceTestAccessor::acquireEntity(*world);
    asteroid.type = types::EntityType::Asteroid;
    asteroid.mass = 100.0;
    asteroid.radius = 2.0;
    asteroid.transform = domain::TransformComponent{};
    asteroid.health = domain::HealthComponent{100, 100};
    asteroid.asteroid = domain::AsteroidComponent{};

    auto& bullet = WorldInstanceTestAccessor::acquireEntity(*world);
    bullet.type = types::EntityType::Bullet;
    bullet.radius = 2.0;
    bullet.transform = domain::TransformComponent{{11.0, 0.0}, {}, 0.0};
    bullet.combat = domain::CombatComponent{12, types::EntityId{999}};

    WorldInstanceTestAccessor::collideShipAsteroid(*world, ship, asteroid);

    EXPECT_EQ(ship.radius, 10.0);
    EXPECT_EQ(ship.health->current, 88);
    EXPECT_TRUE(bullet.isDestroyed);
    EXPECT_TRUE(asteroid.isDestroyed);
}

TEST(WorldInstance, GrowingShipIgnoresItsOwnBullet) {
    boost::asio::io_context io_context;
    auto world = WorldInstance::create(io_context, events::EventBus::create(), 60.0, 1000.0, 1000.0);

    auto& ship = WorldInstanceTestAccessor::acquireEntity(*world);
    ship.type = types::EntityType::Ship;
    ship.mass = 300.0;
    ship.radius = 5.0;
    ship.transform = domain::TransformComponent{};
    ship.health = domain::HealthComponent{100, 100};
    ship.ship = domain::ShipComponent{};

    auto& asteroid = WorldInstanceTestAccessor::acquireEntity(*world);
    asteroid.type = types::EntityType::Asteroid;
    asteroid.mass = 100.0;
    asteroid.radius = 2.0;
    asteroid.transform = domain::TransformComponent{};
    asteroid.health = domain::HealthComponent{100, 100};
    asteroid.asteroid = domain::AsteroidComponent{};

    auto& bullet = WorldInstanceTestAccessor::acquireEntity(*world);
    bullet.type = types::EntityType::Bullet;
    bullet.radius = 2.0;
    bullet.transform = domain::TransformComponent{{11.0, 0.0}, {}, 0.0};
    bullet.combat = domain::CombatComponent{12, ship.id};

    WorldInstanceTestAccessor::collideShipAsteroid(*world, ship, asteroid);

    EXPECT_EQ(ship.health->current, 100);
    EXPECT_FALSE(bullet.isDestroyed);
}

TEST(WorldInstance, CreateWithNullEventBusThrows) {
    boost::asio::io_context io_context;

    EXPECT_THROW(WorldInstance::create(io_context, nullptr), std::invalid_argument);
}

TEST(WorldInstance, RejectsNonPositiveTickRate) {
    boost::asio::io_context io_context;

    EXPECT_THROW(WorldInstance::create(io_context, events::EventBus::create(), 0.0), std::invalid_argument);
}

TEST(WorldInstance, FreePositionReturnsCircleInsideWorld) {
    boost::asio::io_context io_context;
    auto world = WorldInstance::create(io_context, events::EventBus::create(), 60.0, 100.0, 100.0);

    const math::Circle area = world->freePosition();

    // Пустой мир 100x100: наибольшая свободная окружность вписана в мир.
    EXPECT_DOUBLE_EQ(area.center.x, 0.0);
    EXPECT_DOUBLE_EQ(area.center.y, 0.0);
    EXPECT_DOUBLE_EQ(area.radius, 50.0);
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
    // force_stop(), а не graceful stop(): tick-цикл перезаводит steady_timer каждый
    // тик, а world->stop() лишь постит doStop() на strand. При активной каденции
    // graceful nc.stop() (без io_context.stop()) не разбудит воркеры, и nc.wait()
    // зависнет. Тесты проверяют поведение WorldInstance, а не graceful-остановку
    // NetworkContext, поэтому здесь безопасен аварийный сброс очереди.
    nc.force_stop();
    nc.wait();
}

TEST(WorldInstance, EnqueueActionsConcurrentlyWhileRunning) {
    constexpr size_t kThreads = 2;
    constexpr size_t kActionsPerThread = 100;

    NetworkContext nc(1);
    auto world = WorldInstance::create(nc.getContext(), events::EventBus::create(), 100.0);
    world->start();
    nc.start();

    // Дожидаемся фактического Running: world->start() постит doStart() на strand
    // асинхронно, и до его выполнения state_ == Stopped, поэтому enqueueAction()
    // отбрасывал бы действия. Ждём первый тик, чтобы тест проверял именно
    // конкурентное enqueueAction() в работающем мире.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (world->currentTick() == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_GT(world->currentTick(), 0u);

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

    world->stop();
    nc.force_stop();
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

    nc.force_stop();
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

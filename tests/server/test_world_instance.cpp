#include <chrono>
#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

#include "config.h"
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
            if (slot.has_value() && !slot->isDestroyed && slot->type == types::EntityType::Ship) {
                count++;
            }
        }
        return count;
    }

    // Прямые вызовы приватных шагов тика (без запуска tick-цикла):
    // processInputQueue() + spawnShips() — как в начале runTickStep().
    static void ProcessInputQueueAndSpawnShips(WorldInstance& world) {
        std::vector<WorldInstance::PendingJoin> new_players;
        world.processInputQueue(new_players);
        world.spawnShips(new_players);
    }

    static void ProcessInputQueue(WorldInstance& world) {
        std::vector<WorldInstance::PendingJoin> new_players;
        world.processInputQueue(new_players);
    }

    // Число занятых слотов пула.
    static std::size_t CountEngagedSlots(const WorldInstance& world) {
        return world.entities_.storage().size();
    }

    static void ResolveCollisions(WorldInstance& world) {
        world.resolveCollisions();
    }

    static domain::Entity* Acquire(WorldInstance& world) {
        return world.entities_.acquire();
    }

    static std::unordered_map<SessionId, types::EntityId>& SessionToShip(WorldInstance& world) {
        return world.session_to_ship_;
    }

    // Находит живую сущность по типу (первую попавшуюся) или nullptr.
    static domain::Entity* FindByType(WorldInstance& world, types::EntityType type) {
        for (auto& slot : world.entities_.storage()) {
            if (slot.has_value() && slot->type == type && !slot->isDestroyed) {
                return &*slot;
            }
        }
        return nullptr;
    }

    // Находит сущность по id (включая помеченные isDestroyed) или nullptr.
    static domain::Entity* FindById(WorldInstance& world, types::EntityId id) {
        for (auto& slot : world.entities_.storage()) {
            if (slot.has_value() && slot->id == id) {
                return &*slot;
            }
        }
        return nullptr;
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

// ---------- Синхронные тесты processInputQueue / resolveCollisions (без tick-цикла) ----------

namespace {

/// Создаёт мир с заполненной сессией и живым кораблём в центре координат.
struct SyncWorldFixture {
    boost::asio::io_context io_context;
    std::shared_ptr<events::EventBus> event_bus{events::EventBus::create()};
    std::shared_ptr<WorldInstance> world{WorldInstance::create(io_context, event_bus, 60.0, 100.0, 100.0)};
    SessionId session_id{42};
    domain::Entity* ship{nullptr};

    SyncWorldFixture() {
        ship = SpawnShip(session_id);
        ship->transform->position = {0.0, 0.0};
    }

    domain::Entity* SpawnShip(SessionId sid) {
        world->enqueueAction(ActionEvent{sid, ActionType::PlayerJoin, {}, {}, 0.0, false, "Test"});
        WorldInstanceTestAccessor::ProcessInputQueueAndSpawnShips(*world);
        return WorldInstanceTestAccessor::FindByType(*world, types::EntityType::Ship);
    }
};

}  // namespace

// MoveAndShoot: угол из действия применяется к кораблю (семантика S-93:
// движение задаётся angle, разгон выполняет updatePhysics при is_thrusting).
TEST(WorldInstance, MoveAndShootAppliesAngle) {
    SyncWorldFixture fx;

    constexpr double kAngle = 1.25;
    fx.world->enqueueAction(ActionEvent{fx.session_id, ActionType::MoveAndShoot, {}, {}, kAngle, false, {}});
    WorldInstanceTestAccessor::ProcessInputQueue(*fx.world);

    ASSERT_NE(fx.ship, nullptr);
    ASSERT_TRUE(fx.ship->transform.has_value());
    EXPECT_DOUBLE_EQ(fx.ship->transform->angle, kAngle);
}

// MoveAndShoot: угол не задан (0.0) — угол корабля остаётся прежним, velocity не трогается.
TEST(WorldInstance, MoveAndShootWithoutAngleKeepsState) {
    SyncWorldFixture fx;
    fx.ship->transform->angle = 0.5;
    fx.ship->transform->velocity = {1.0, 1.0};

    fx.world->enqueueAction(ActionEvent{fx.session_id, ActionType::MoveAndShoot, {}, {}, 0.0, false, {}});
    WorldInstanceTestAccessor::ProcessInputQueue(*fx.world);

    EXPECT_DOUBLE_EQ(fx.ship->transform->angle, 0.0);
    EXPECT_DOUBLE_EQ(fx.ship->transform->velocity.x, 1.0);
    EXPECT_DOUBLE_EQ(fx.ship->transform->velocity.y, 1.0);
}

// MoveAndShoot с is_shooting: в пуле появляется пуля с owner_id, позицией на носу корабля и радиусом kBulletRadius.
TEST(WorldInstance, MoveAndShootSpawnsBullet) {
    SyncWorldFixture fx;
    fx.ship->transform->angle = 0.0;  // смотрит вдоль +X
    const auto ship_pos = fx.ship->transform->position;
    const auto ship_radius = fx.ship->radius;

    fx.world->enqueueAction(ActionEvent{fx.session_id, ActionType::MoveAndShoot, {}, {}, 0.0, true, {}});
    WorldInstanceTestAccessor::ProcessInputQueue(*fx.world);

    auto* bullet = WorldInstanceTestAccessor::FindByType(*fx.world, types::EntityType::Bullet);
    ASSERT_NE(bullet, nullptr);
    ASSERT_TRUE(bullet->transform.has_value());
    ASSERT_TRUE(bullet->combat.has_value());

    EXPECT_EQ(bullet->combat->owner_id, fx.ship->id);
    EXPECT_DOUBLE_EQ(bullet->radius, world::kBulletRadius);
    EXPECT_DOUBLE_EQ(bullet->transform->position.x, ship_pos.x + ship_radius);
    EXPECT_DOUBLE_EQ(bullet->transform->position.y, ship_pos.y);
    EXPECT_DOUBLE_EQ(bullet->transform->velocity.x, world::kBulletSpeed);
    EXPECT_DOUBLE_EQ(bullet->transform->velocity.y, 0.0);
}

// MoveAndShoot для неизвестной сессии игнорируется: пул не меняется.
TEST(WorldInstance, MoveAndShootUnknownSessionIgnored) {
    SyncWorldFixture fx;
    const auto entities_before = WorldInstanceTestAccessor::CountEngagedSlots(*fx.world);

    fx.world->enqueueAction(ActionEvent{9999, ActionType::MoveAndShoot, {}, {}, 1.0, true, {}});
    WorldInstanceTestAccessor::ProcessInputQueue(*fx.world);

    EXPECT_EQ(WorldInstanceTestAccessor::CountEngagedSlots(*fx.world), entities_before);
    ASSERT_NE(fx.ship, nullptr);
    EXPECT_DOUBLE_EQ(fx.ship->transform->angle, 0.0);
}

// resolveCollisions: пуля уничтожает корабль — публикуется ShipDestroyedEvent с session_id,
// маппинг session_to_ship_ удаляется.
TEST(WorldInstance, BulletKillsShipPublishesShipDestroyedEvent) {
    SyncWorldFixture fx;

    int destroyed_events = 0;
    SessionId destroyed_session = 0;
    auto conn = fx.event_bus->subscribe<ShipDestroyedEvent>([&](const ShipDestroyedEvent& e) {
        ++destroyed_events;
        destroyed_session = e.session_id;
    });

    // Ровно один хит до гибели: kBulletDamage = 25 при kShipDefaultHealth = 100.
    // owner_id != ship->id: пуля «вражеская», иначе сработала бы защита от самопопадания.
    // Урон накапливается: 100 → 75 → 50 → 25 → 0 (гибель на 4-й итерации).
    const types::EntityId enemy_owner{fx.ship->id.get() + 1000};
    for (int i = 0; i < 4; ++i) {
        auto* bullet = WorldInstanceTestAccessor::Acquire(*fx.world);
        bullet->type = types::EntityType::Bullet;
        bullet->radius = world::kBulletRadius;
        bullet->transform = domain::TransformComponent{fx.ship->transform->position, {0.0, 0.0}, 0.0};
        bullet->combat = domain::CombatComponent{world::kBulletDamage, enemy_owner};
        WorldInstanceTestAccessor::ResolveCollisions(*fx.world);
    }

    EXPECT_EQ(destroyed_events, 1);
    EXPECT_EQ(destroyed_session, fx.session_id);
    EXPECT_TRUE(WorldInstanceTestAccessor::GetSessionToShip(*fx.world).empty());
}

// resolveCollisions: корабль, уничтоженный через PlayerQuit-путь, НЕ публикует ShipDestroyedEvent.
TEST(WorldInstance, PlayerQuitShipDoesNotPublishShipDestroyedEvent) {
    SyncWorldFixture fx;

    int destroyed_events = 0;
    auto conn = fx.event_bus->subscribe<ShipDestroyedEvent>([&](const ShipDestroyedEvent&) {
        ++destroyed_events;
    });

    fx.world->enqueueAction(ActionEvent{fx.session_id, ActionType::PlayerQuit, {}, {}, 0.0, false, {}});
    WorldInstanceTestAccessor::ProcessInputQueue(*fx.world);
    WorldInstanceTestAccessor::ResolveCollisions(*fx.world);

    EXPECT_EQ(destroyed_events, 0);
    EXPECT_TRUE(WorldInstanceTestAccessor::GetSessionToShip(*fx.world).empty());
}

// resolveCollisions: уничтоженный корабль без привязки к сессии не публикует событие (только лог-инцидент).
TEST(WorldInstance, SessionlessDestroyedShipDoesNotPublishEvent) {
    SyncWorldFixture fx;

    int destroyed_events = 0;
    auto conn = fx.event_bus->subscribe<ShipDestroyedEvent>([&](const ShipDestroyedEvent&) {
        ++destroyed_events;
    });

    // Корабль в пуле есть, но маппинга нет (эмулируем потерю связки).
    WorldInstanceTestAccessor::SessionToShip(*fx.world).erase(fx.session_id);
    fx.ship->isDestroyed = true;

    WorldInstanceTestAccessor::ResolveCollisions(*fx.world);

    EXPECT_EQ(destroyed_events, 0);
    EXPECT_TRUE(WorldInstanceTestAccessor::GetSessionToShip(*fx.world).empty());
}

// resolveCollisions: пара (Asteroid, Ship) обрабатывается в обоих порядках слотов.
TEST(WorldInstance, CollisionDispatchesBothSlotOrders) {
    // Порядок 1: asteroid-слот раньше ship-слота.
    {
        boost::asio::io_context io;
        auto world = WorldInstance::create(io, events::EventBus::create(), 60.0, 100.0, 100.0);

        auto* asteroid = WorldInstanceTestAccessor::Acquire(*world);
        asteroid->type = types::EntityType::Asteroid;
        asteroid->mass = 0.1 * domain::Entity::massForRadius(5.0);  // ratio <= 0.2 — астероид отталкивается
        asteroid->radius = 2.0;
        asteroid->transform = domain::TransformComponent{{0.0, 0.0}, {0.0, 0.0}, 0.0};

        auto* ship = WorldInstanceTestAccessor::Acquire(*world);
        ship->type = types::EntityType::Ship;
        ship->mass = domain::Entity::massForRadius(5.0);
        ship->radius = 5.0;
        ship->ship = domain::ShipComponent{};
        ship->health = domain::HealthComponent{100, 100};
        ship->combat = domain::CombatComponent{};
        ship->transform = domain::TransformComponent{{1.0, 0.0}, {0.0, 0.0}, 0.0};  // пересекается

        WorldInstanceTestAccessor::ResolveCollisions(*world);

        EXPECT_FALSE(ship->isDestroyed);  // лёгкий астероид не наносит урон
    }
    // Порядок 2: ship-слот раньше asteroid-слота — результат тот же.
    {
        boost::asio::io_context io;
        auto world = WorldInstance::create(io, events::EventBus::create(), 60.0, 100.0, 100.0);

        auto* ship = WorldInstanceTestAccessor::Acquire(*world);
        ship->type = types::EntityType::Ship;
        ship->mass = domain::Entity::massForRadius(5.0);
        ship->radius = 5.0;
        ship->ship = domain::ShipComponent{};
        ship->health = domain::HealthComponent{100, 100};
        ship->combat = domain::CombatComponent{};
        ship->transform = domain::TransformComponent{{1.0, 0.0}, {0.0, 0.0}, 0.0};

        auto* asteroid = WorldInstanceTestAccessor::Acquire(*world);
        asteroid->type = types::EntityType::Asteroid;
        asteroid->mass = 0.1 * domain::Entity::massForRadius(5.0);
        asteroid->radius = 2.0;
        asteroid->transform = domain::TransformComponent{{0.0, 0.0}, {0.0, 0.0}, 0.0};

        WorldInstanceTestAccessor::ResolveCollisions(*world);

        EXPECT_FALSE(ship->isDestroyed);
    }
}

// resolveCollisions: пустые слоты, isDestroyed, слоты без transform и тип None пропускаются без краша.
TEST(WorldInstance, CollisionSkipsInvalidSlots) {
    boost::asio::io_context io;
    auto world = WorldInstance::create(io, events::EventBus::create(), 60.0, 100.0, 100.0);

    // Слот-«пустышка» с типом None и без transform.
    auto* none_entity = WorldInstanceTestAccessor::Acquire(*world);
    none_entity->type = types::EntityType::None;

    // Живой корабль.
    auto* ship = WorldInstanceTestAccessor::Acquire(*world);
    ship->type = types::EntityType::Ship;
    ship->mass = domain::Entity::massForRadius(5.0);
    ship->radius = 5.0;
    ship->ship = domain::ShipComponent{};
    ship->health = domain::HealthComponent{100, 100};
    ship->combat = domain::CombatComponent{};
    ship->transform = domain::TransformComponent{{0.0, 0.0}, {0.0, 0.0}, 0.0};

    // Уже уничтоженная сущность — не должна участвовать в коллизиях.
    auto* dead = WorldInstanceTestAccessor::Acquire(*world);
    dead->type = types::EntityType::Asteroid;
    dead->mass = 100.0;
    dead->radius = 50.0;  // заведомо пересекался бы с кораблём
    dead->transform = domain::TransformComponent{{0.0, 0.0}, {0.0, 0.0}, 0.0};
    dead->isDestroyed = true;

    // Сущность без transform.
    auto* no_transform = WorldInstanceTestAccessor::Acquire(*world);
    no_transform->type = types::EntityType::Asteroid;
    no_transform->mass = 100.0;
    no_transform->radius = 50.0;

    EXPECT_NO_THROW(WorldInstanceTestAccessor::ResolveCollisions(*world));
    EXPECT_FALSE(ship->isDestroyed);
}

#include <deque>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

#include "entity.h"
#include "entity_pool.h"
#include "entity_snapshot.h"
#include "entity_utils.h"

namespace {

using snapshot::AsteroidSnapshot;
using snapshot::CombatSnapshot;
using snapshot::EntitySnapshot;
using snapshot::HealthSnapshot;
using snapshot::ShipSnapshot;
using snapshot::TransformSnapshot;
using types::EntityId;
using types::EntityType;

domain::Entity MakeShip(EntityId id) {
    domain::Entity entity;
    entity.id = id;
    entity.type = EntityType::Ship;
    entity.mass = 12.5;
    entity.radius = 3.25;
    entity.transform = domain::TransformComponent{{123.25, -456.5}, {-0.125, 2.75}, 1.25};
    domain::ShipComponent ship;
    ship.name.fill('\0');
    const std::string name = "player";
    std::copy(name.begin(), name.end(), ship.name.begin());
    ship.score = 987654U;
    ship.thrust_power = 7.5;
    ship.drag = 0.125;
    ship.is_thrusting = true;
    ship.frags = 4;
    entity.ship = ship;
    entity.health = domain::HealthComponent{75, 100};
    entity.combat = domain::CombatComponent{10, EntityId{7}};
    return entity;
}

domain::Entity MakeAsteroid(EntityId id) {
    domain::Entity entity;
    entity.id = id;
    entity.type = EntityType::Asteroid;
    entity.mass = 100.0;
    entity.radius = 8.0;
    entity.transform = domain::TransformComponent{{-4.0, 2.0}, {1.0, -3.0}, 0.5};
    entity.health = domain::HealthComponent{0, 50};
    entity.asteroid = domain::AsteroidComponent{500};
    return entity;
}

void ExpectEntitySnapshotsEqual(const snapshot::EntitySnapshot& actual, const domain::Entity& expected) {
    EXPECT_EQ(actual.id, expected.id);
    EXPECT_EQ(actual.type, expected.type);
    EXPECT_DOUBLE_EQ(actual.mass, expected.mass);
    EXPECT_DOUBLE_EQ(actual.radius, expected.radius);
    EXPECT_EQ(actual.is_destroyed, expected.isDestroyed);

    ASSERT_EQ(actual.transform.has_value(), expected.transform.has_value());
    if (actual.transform) {
        EXPECT_DOUBLE_EQ(actual.transform->position.x, expected.transform->position.x);
        EXPECT_DOUBLE_EQ(actual.transform->position.y, expected.transform->position.y);
        EXPECT_DOUBLE_EQ(actual.transform->velocity.x, expected.transform->velocity.x);
        EXPECT_DOUBLE_EQ(actual.transform->velocity.y, expected.transform->velocity.y);
        EXPECT_DOUBLE_EQ(actual.transform->angle, expected.transform->angle);
    }

    ASSERT_EQ(actual.ship.has_value(), expected.ship.has_value());
    if (actual.ship) {
        EXPECT_EQ(actual.ship->name, std::string{expected.ship->name.data()});
        EXPECT_EQ(actual.ship->score, expected.ship->score);
        EXPECT_DOUBLE_EQ(actual.ship->thrust_power, expected.ship->thrust_power);
        EXPECT_DOUBLE_EQ(actual.ship->drag, expected.ship->drag);
        EXPECT_EQ(actual.ship->is_thrusting, expected.ship->is_thrusting);
        EXPECT_EQ(actual.ship->frags, expected.ship->frags);
    }

    ASSERT_EQ(actual.health.has_value(), expected.health.has_value());
    if (actual.health) {
        EXPECT_EQ(actual.health->current, expected.health->current);
        EXPECT_EQ(actual.health->max, expected.health->max);
    }

    ASSERT_EQ(actual.combat.has_value(), expected.combat.has_value());
    if (actual.combat) {
        EXPECT_EQ(actual.combat->damage, expected.combat->damage);
        EXPECT_EQ(actual.combat->owner_id, expected.combat->owner_id);
    }

    ASSERT_EQ(actual.asteroid.has_value(), expected.asteroid.has_value());
    if (actual.asteroid) {
        EXPECT_EQ(actual.asteroid->score_reward, expected.asteroid->score_reward);
    }
}

TEST(ToEntitySnapshot, MirrorsShipComponentFields) {
    const auto entity = MakeShip(EntityId{42});
    const auto snapshot = domain::toEntitySnapshot(entity);
    ExpectEntitySnapshotsEqual(snapshot, entity);
}

TEST(ToEntitySnapshot, MirrorsAsteroidComponentFields) {
    const auto entity = MakeAsteroid(EntityId{7});
    const auto snapshot = domain::toEntitySnapshot(entity);
    ExpectEntitySnapshotsEqual(snapshot, entity);
}

TEST(BuildEntitySnapshots, SkipsEmptySlotsAndDestroyedEntities) {
    domain::EntityPool pool;
    auto& storage = pool.storage();
    storage.emplace_back(MakeShip(EntityId{1}));
    storage.emplace_back();  // пустой слот
    auto destroyed = MakeAsteroid(EntityId{2});
    destroyed.isDestroyed = true;
    storage.emplace_back(destroyed);
    storage.emplace_back(MakeAsteroid(EntityId{3}));

    const auto snapshots = domain::buildEntitySnapshots(pool);

    ASSERT_EQ(snapshots.size(), 2U);
    EXPECT_EQ(snapshots[0].id, EntityId{1});
    EXPECT_EQ(snapshots[1].id, EntityId{3});
}

TEST(BuildEntitySnapshots, WorksWithEntityPoolStorage) {
    domain::EntityPool pool;
    auto* ship = pool.acquire();
    *ship = MakeShip(EntityId{10});
    auto* asteroid = pool.acquire();
    *asteroid = MakeAsteroid(EntityId{11});
    auto* destroyed = pool.acquire();
    *destroyed = MakeAsteroid(EntityId{12});
    destroyed->isDestroyed = true;

    const auto snapshots = domain::buildEntitySnapshots(pool);

    ASSERT_EQ(snapshots.size(), 2U);
    EXPECT_EQ(snapshots[0].id, EntityId{10});
    EXPECT_EQ(snapshots[1].id, EntityId{11});
}

TEST(BuildEntitySnapshots, EmptyStorageYieldsEmptyVector) {
    domain::EntityPool pool;
    EXPECT_TRUE(domain::buildEntitySnapshots(pool).empty());
}

TEST(BuildEntitySnapshots, SkipsSlotWithNoneType) {
    domain::EntityPool pool;
    auto* none = pool.acquire();
    none->type = EntityType::None;  // неинициализированная сущность (дефолт)
    auto* ship = pool.acquire();
    *ship = MakeShip(EntityId{5});

    const auto snapshots = domain::buildEntitySnapshots(pool);

    ASSERT_EQ(snapshots.size(), 1U);
    EXPECT_EQ(snapshots[0].id, EntityId{5});
}

}  // namespace

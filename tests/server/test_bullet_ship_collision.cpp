#include <cstdint>
#include <gtest/gtest.h>

#include "collision_utils.h"

namespace {

domain::Entity createBullet(types::EntityId owner_id = types::EntityId{1}) {
    domain::Entity bullet;
    bullet.type = types::EntityType::Bullet;
    bullet.radius = 2.0;
    bullet.transform.emplace();
    bullet.combat.emplace(domain::CombatComponent{25, owner_id});

    return bullet;
}

domain::Entity createShip(std::uint16_t health) {
    domain::Entity ship;
    ship.type = types::EntityType::Ship;
    ship.radius = 5.0;
    ship.transform.emplace();
    ship.health.emplace(domain::HealthComponent{health, 100});
    ship.ship.emplace();

    return ship;
}

TEST(BulletShipCollisionTest, BulletDealsTwentyFiveDamage) {
    auto bullet = createBullet();
    auto ship = createShip(100);

    bool callback_called = false;

    domain::collision::onCollisionBulletShip(bullet, ship, [&](types::EntityId, bool) {
        callback_called = true;
    });

    EXPECT_EQ(ship.health->current, 75);
    EXPECT_TRUE(bullet.isDestroyed);
    EXPECT_FALSE(ship.isDestroyed);
    EXPECT_TRUE(callback_called);
}

TEST(BulletShipCollisionTest, BulletDestroysShipWithTwentyFiveHealth) {
    auto bullet = createBullet();
    auto ship = createShip(25);

    bool ship_destroyed = false;

    domain::collision::onCollisionBulletShip(bullet, ship, [&](types::EntityId, bool destroyed) {
        ship_destroyed = destroyed;
    });

    EXPECT_EQ(ship.health->current, 0);
    EXPECT_TRUE(bullet.isDestroyed);
    EXPECT_TRUE(ship.isDestroyed);
    EXPECT_TRUE(ship_destroyed);
}

TEST(BulletShipCollisionTest, ShipSurvivesWhenHealthIsAboveDamage) {
    auto bullet = createBullet();
    auto ship = createShip(26);

    bool ship_destroyed = true;

    domain::collision::onCollisionBulletShip(bullet, ship, [&](types::EntityId, bool destroyed) {
        ship_destroyed = destroyed;
    });

    EXPECT_EQ(ship.health->current, 1);
    EXPECT_TRUE(bullet.isDestroyed);
    EXPECT_FALSE(ship.isDestroyed);
    EXPECT_FALSE(ship_destroyed);
}

TEST(BulletShipCollisionTest, CallbackReceivesBulletOwnerId) {
    const types::EntityId owner_id{42};
    auto bullet = createBullet(owner_id);
    auto ship = createShip(100);

    types::EntityId actual_owner_id{};

    domain::collision::onCollisionBulletShip(bullet, ship, [&](types::EntityId id, bool) {
        actual_owner_id = id;
    });

    EXPECT_EQ(actual_owner_id, owner_id);
}

TEST(BulletShipCollisionTest, InvalidBulletTypeDoesNotCauseCollision) {
    auto bullet = createBullet();
    bullet.type = types::EntityType::Asteroid;
    auto ship = createShip(100);

    bool callback_called = false;

    domain::collision::onCollisionBulletShip(bullet, ship, [&](types::EntityId, bool) {
        callback_called = true;
    });

    EXPECT_EQ(ship.health->current, 100);
    EXPECT_FALSE(bullet.isDestroyed);
    EXPECT_FALSE(callback_called);
}

TEST(BulletShipCollisionTest, BulletWithoutCombatDoesNotCauseCollision) {
    auto bullet = createBullet();
    bullet.combat.reset();
    auto ship = createShip(100);

    bool callback_called = false;

    domain::collision::onCollisionBulletShip(bullet, ship, [&](types::EntityId, bool) {
        callback_called = true;
    });

    EXPECT_EQ(ship.health->current, 100);
    EXPECT_FALSE(bullet.isDestroyed);
    EXPECT_FALSE(callback_called);
}

TEST(BulletShipCollisionTest, DestroyedBulletDoesNotCauseCollision) {
    auto bullet = createBullet();
    bullet.isDestroyed = true;
    auto ship = createShip(100);

    bool callback_called = false;

    domain::collision::onCollisionBulletShip(bullet, ship, [&](types::EntityId, bool) {
        callback_called = true;
    });

    EXPECT_EQ(ship.health->current, 100);
    EXPECT_FALSE(callback_called);
}

TEST(BulletShipCollisionTest, ShipWithoutHealthDoesNotCauseCollision) {
    auto bullet = createBullet();
    auto ship = createShip(100);
    ship.health.reset();

    bool callback_called = false;

    domain::collision::onCollisionBulletShip(bullet, ship, [&](types::EntityId, bool) {
        callback_called = true;
    });

    EXPECT_FALSE(bullet.isDestroyed);
    EXPECT_FALSE(callback_called);
}

}  // namespace

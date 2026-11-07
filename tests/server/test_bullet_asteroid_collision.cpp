#include <cstdint>
#include <gtest/gtest.h>

#include "collision_utils.h"

namespace {

domain::Entity createBullet(types::EntityId owner_id = types::EntityId{1}, std::uint16_t damage = 25) {
    domain::Entity bullet;
    bullet.type = types::EntityType::Bullet;
    bullet.radius = 2.0;
    bullet.transform.emplace();
    bullet.combat.emplace(domain::CombatComponent{damage, owner_id});

    return bullet;
}

domain::Entity createAsteroid(std::uint16_t xp) {
    domain::Entity asteroid;
    asteroid.type = types::EntityType::Asteroid;
    asteroid.radius = 5.0;
    asteroid.transform.emplace();
    asteroid.health.emplace(domain::HealthComponent{xp, xp});
    asteroid.asteroid.emplace();

    return asteroid;
}

TEST(BulletAsteroidCollisionTest, BulletDealsTwentyFiveDamage) {
    auto bullet = createBullet();
    auto asteroid = createAsteroid(100);

    bool callback_called = false;

    domain::collision::onCollisionBulletAsteroid(bullet, asteroid, [&](types::EntityId, bool) {
        callback_called = true;
    });

    EXPECT_EQ(asteroid.health->current, 75);
    EXPECT_TRUE(bullet.isDestroyed);
    EXPECT_FALSE(asteroid.isDestroyed);
    EXPECT_TRUE(callback_called);
}

TEST(BulletAsteroidCollisionTest, UsesDamageFromCombatComponent) {
    auto bullet = createBullet(types::EntityId{1}, 10);
    auto asteroid = createAsteroid(100);

    domain::collision::onCollisionBulletAsteroid(bullet, asteroid, [](types::EntityId, bool) {
    });

    EXPECT_EQ(asteroid.health->current, 90);
    EXPECT_TRUE(bullet.isDestroyed);
    EXPECT_FALSE(asteroid.isDestroyed);
}

TEST(BulletAsteroidCollisionTest, BulletDestroysAsteroidWithTwentyFiveXp) {
    auto bullet = createBullet();
    auto asteroid = createAsteroid(25);

    bool asteroid_destroyed = false;

    domain::collision::onCollisionBulletAsteroid(bullet, asteroid, [&](types::EntityId, bool destroyed) {
        asteroid_destroyed = destroyed;
    });

    EXPECT_EQ(asteroid.health->current, 0);
    EXPECT_TRUE(bullet.isDestroyed);
    EXPECT_TRUE(asteroid.isDestroyed);
    EXPECT_TRUE(asteroid_destroyed);
}

TEST(BulletAsteroidCollisionTest, BulletDestroysAsteroidWithLessThanTwentyFiveXp) {
    auto bullet = createBullet();
    auto asteroid = createAsteroid(10);

    bool asteroid_destroyed = false;

    domain::collision::onCollisionBulletAsteroid(bullet, asteroid, [&](types::EntityId, bool destroyed) {
        asteroid_destroyed = destroyed;
    });

    EXPECT_EQ(asteroid.health->current, 0);
    EXPECT_TRUE(bullet.isDestroyed);
    EXPECT_TRUE(asteroid.isDestroyed);
    EXPECT_TRUE(asteroid_destroyed);
}

TEST(BulletAsteroidCollisionTest, AsteroidSurvivesWhenXpIsAboveDamage) {
    auto bullet = createBullet();
    auto asteroid = createAsteroid(26);

    bool asteroid_destroyed = true;

    domain::collision::onCollisionBulletAsteroid(bullet, asteroid, [&](types::EntityId, bool destroyed) {
        asteroid_destroyed = destroyed;
    });

    EXPECT_EQ(asteroid.health->current, 1);
    EXPECT_TRUE(bullet.isDestroyed);
    EXPECT_FALSE(asteroid.isDestroyed);
    EXPECT_FALSE(asteroid_destroyed);
}

TEST(BulletAsteroidCollisionTest, CallbackReceivesBulletOwnerId) {
    const types::EntityId owner_id{42};
    auto bullet = createBullet(owner_id);
    auto asteroid = createAsteroid(100);

    types::EntityId actual_owner_id{};

    domain::collision::onCollisionBulletAsteroid(bullet, asteroid, [&](types::EntityId id, bool) {
        actual_owner_id = id;
    });

    EXPECT_EQ(actual_owner_id, owner_id);
}

TEST(BulletAsteroidCollisionTest, InvalidBulletTypeDoesNotCauseCollision) {
    auto bullet = createBullet();
    bullet.type = types::EntityType::Ship;
    auto asteroid = createAsteroid(100);

    bool callback_called = false;

    domain::collision::onCollisionBulletAsteroid(bullet, asteroid, [&](types::EntityId, bool) {
        callback_called = true;
    });

    EXPECT_EQ(asteroid.health->current, 100);
    EXPECT_FALSE(bullet.isDestroyed);
    EXPECT_FALSE(callback_called);
}

TEST(BulletAsteroidCollisionTest, BulletWithoutCombatDoesNotCauseCollision) {
    auto bullet = createBullet();
    bullet.combat.reset();
    auto asteroid = createAsteroid(100);

    bool callback_called = false;

    domain::collision::onCollisionBulletAsteroid(bullet, asteroid, [&](types::EntityId, bool) {
        callback_called = true;
    });

    EXPECT_EQ(asteroid.health->current, 100);
    EXPECT_FALSE(bullet.isDestroyed);
    EXPECT_FALSE(callback_called);
}

TEST(BulletAsteroidCollisionTest, DestroyedBulletDoesNotCauseCollision) {
    auto bullet = createBullet();
    bullet.isDestroyed = true;
    auto asteroid = createAsteroid(100);

    bool callback_called = false;

    domain::collision::onCollisionBulletAsteroid(bullet, asteroid, [&](types::EntityId, bool) {
        callback_called = true;
    });

    EXPECT_EQ(asteroid.health->current, 100);
    EXPECT_FALSE(callback_called);
}

TEST(BulletAsteroidCollisionTest, AsteroidWithoutHealthDoesNotCauseCollision) {
    auto bullet = createBullet();
    auto asteroid = createAsteroid(100);
    asteroid.health.reset();

    bool callback_called = false;

    domain::collision::onCollisionBulletAsteroid(bullet, asteroid, [&](types::EntityId, bool) {
        callback_called = true;
    });

    EXPECT_FALSE(bullet.isDestroyed);
    EXPECT_FALSE(callback_called);
}

TEST(BulletAsteroidCollisionTest, AsteroidWithoutAsteroidComponentDoesNotCauseCollision) {
    auto bullet = createBullet();
    auto asteroid = createAsteroid(100);
    asteroid.asteroid.reset();

    bool callback_called = false;

    domain::collision::onCollisionBulletAsteroid(bullet, asteroid, [&](types::EntityId, bool) {
        callback_called = true;
    });

    EXPECT_EQ(asteroid.health->current, 100);
    EXPECT_FALSE(bullet.isDestroyed);
    EXPECT_FALSE(callback_called);
}

TEST(BulletAsteroidCollisionTest, HandlesReversedArgumentOrder) {
    const types::EntityId owner_id{42};
    auto bullet = createBullet(owner_id);
    auto asteroid = createAsteroid(25);

    bool callback_called = false;
    types::EntityId actual_owner_id{};
    bool asteroid_destroyed = false;

    domain::collision::onCollisionBulletAsteroid(asteroid, bullet, [&](types::EntityId id, bool destroyed) {
        callback_called = true;
        actual_owner_id = id;
        asteroid_destroyed = destroyed;
    });

    EXPECT_EQ(asteroid.health->current, 0);
    EXPECT_TRUE(bullet.isDestroyed);
    EXPECT_TRUE(asteroid.isDestroyed);
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(actual_owner_id, owner_id);
    EXPECT_TRUE(asteroid_destroyed);
}

TEST(BulletAsteroidCollisionTest, AsteroidWithZeroXpDoesNotCauseCollision) {
    auto bullet = createBullet();
    auto asteroid = createAsteroid(0);

    bool callback_called = false;

    domain::collision::onCollisionBulletAsteroid(bullet, asteroid, [&](types::EntityId, bool) {
        callback_called = true;
    });

    EXPECT_FALSE(bullet.isDestroyed);
    EXPECT_FALSE(callback_called);
}

}  // namespace

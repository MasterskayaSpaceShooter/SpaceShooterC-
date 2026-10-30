#include <gtest/gtest.h>
#include <limits>

#include "collision_utils.h"

namespace {

domain::Entity MakeShip(math::Scalar mass = 200.0) {
    domain::Entity ship;
    ship.type = types::EntityType::Ship;
    ship.mass = mass;
    ship.radius = 10.0;
    ship.transform = domain::TransformComponent{};
    ship.ship = domain::ShipComponent{};
    ship.health = domain::HealthComponent{80, 100};
    return ship;
}

domain::Entity MakeAsteroid(math::Scalar mass = 100.0) {
    domain::Entity asteroid;
    asteroid.type = types::EntityType::Asteroid;
    asteroid.mass = mass;
    asteroid.transform = domain::TransformComponent{};
    asteroid.asteroid = domain::AsteroidComponent{};
    asteroid.health = domain::HealthComponent{60, 80};
    return asteroid;
}

TEST(ShipAsteroidCollisionTest, RepelStubDoesNotChangeVelocities) {
    auto ship = MakeShip(500.0);
    auto asteroid = MakeAsteroid(100.0);
    ship.transform->velocity = {10.0, 0.0};
    asteroid.transform->position = {10.0, 0.0};

    domain::collision::onCollision(ship, asteroid);

    EXPECT_FALSE(asteroid.isDestroyed);
    EXPECT_EQ(ship.health->current, 80);
    EXPECT_EQ(asteroid.health->current, 60);
    EXPECT_EQ(ship.transform->velocity, (math::Vector2D{10.0, 0.0}));
    EXPECT_EQ(asteroid.transform->velocity, (math::Vector2D{}));
}

TEST(ShipAsteroidCollisionTest, EatsAsteroidBelowHalfShipMass) {
    auto ship = MakeShip(200.0);
    auto asteroid = MakeAsteroid(99.0);
    asteroid.asteroid->score_reward = 15;

    domain::collision::onCollision(ship, asteroid);

    EXPECT_TRUE(asteroid.isDestroyed);
    EXPECT_DOUBLE_EQ(ship.mass, 299.0);
    EXPECT_EQ(ship.ship->score, 15u);
    EXPECT_EQ(ship.health->current, 80);
}

TEST(ShipAsteroidCollisionTest, GrowsShipRadiusWhenMassReachesNextLevel) {
    auto ship = MakeShip(domain::Entity::massForRadius(domain::Entity::kInitialShipRadius));
    ship.radius = domain::Entity::kInitialShipRadius;
    ship.transform->position = {2.0, -3.0};
    ship.transform->velocity = {4.0, 5.0};

    for (const auto asteroid_mass : {19.0, 28.0, 40.0}) {
        auto asteroid = MakeAsteroid(asteroid_mass);
        domain::collision::onCollision(ship, asteroid);
        EXPECT_TRUE(asteroid.isDestroyed);
        EXPECT_DOUBLE_EQ(ship.radius, domain::Entity::kInitialShipRadius);
    }

    auto asteroid = MakeAsteroid(31.0);
    domain::collision::onCollision(ship, asteroid);

    EXPECT_TRUE(asteroid.isDestroyed);
    EXPECT_DOUBLE_EQ(ship.radius, domain::Entity::kInitialShipRadius * 2.0);
    EXPECT_EQ(ship.transform->position, (math::Vector2D{2.0, -3.0}));
    EXPECT_EQ(ship.transform->velocity, (math::Vector2D{4.0, 5.0}));
}

TEST(ShipAsteroidCollisionTest, GrowsMultipleLevelsWhenMassIsAddedAtOnce) {
    auto ship = MakeShip(domain::Entity::massForRadius(domain::Entity::kInitialShipRadius));
    ship.radius = domain::Entity::kInitialShipRadius;

    EXPECT_TRUE(ship.addMass(10000.0));

    EXPECT_DOUBLE_EQ(ship.radius, domain::Entity::kInitialShipRadius * 8.0);
}

TEST(ShipAsteroidCollisionTest, EatingAsteroidAtWorldEdgeDoesNotGrowOutsideBounds) {
    auto ship = MakeShip(130.0);
    ship.radius = domain::Entity::kInitialShipRadius;
    ship.transform->position = {4.0, 0.0};
    ship.transform->velocity = {3.0, 2.0};
    auto asteroid = MakeAsteroid(40.0);
    asteroid.asteroid->score_reward = 7;

    constexpr math::Scalar half_width = 10.0;
    constexpr math::Scalar half_height = 10.0;
    const auto can_grow = [=](const math::Vector2D& position, math::Scalar radius) noexcept {
        return position.x - radius >= -half_width && position.x + radius <= half_width &&
               position.y - radius >= -half_height && position.y + radius <= half_height;
    };

    domain::collision::onCollision(ship, asteroid, can_grow);

    EXPECT_TRUE(asteroid.isDestroyed);
    EXPECT_DOUBLE_EQ(ship.mass, 170.0);
    EXPECT_DOUBLE_EQ(ship.radius, domain::Entity::kInitialShipRadius);
    EXPECT_EQ(ship.ship->score, 7u);
    EXPECT_EQ(ship.transform->position, (math::Vector2D{4.0, 0.0}));
    EXPECT_EQ(ship.transform->velocity, (math::Vector2D{3.0, 2.0}));
}

TEST(ShipAsteroidCollisionTest, DoesNotApplyDamageAtHalfShipMassBoundary) {
    auto ship = MakeShip(200.0);
    auto asteroid = MakeAsteroid(100.0);

    domain::collision::onCollision(ship, asteroid);

    EXPECT_FALSE(asteroid.isDestroyed);
    EXPECT_EQ(ship.health->current, 80);
    EXPECT_EQ(asteroid.health->current, 60);
}

TEST(ShipAsteroidCollisionTest, DoesNotApplyDamageWhenAsteroidMassEqualsShipMass) {
    auto ship = MakeShip(200.0);
    auto asteroid = MakeAsteroid(200.0);

    domain::collision::onCollision(ship, asteroid);

    EXPECT_EQ(ship.health->current, 80);
    EXPECT_EQ(asteroid.health->current, 60);
}

TEST(ShipAsteroidCollisionTest, DoesNotApplyDamageWhenAsteroidIsHeavier) {
    auto ship = MakeShip(200.0);
    auto asteroid = MakeAsteroid(201.0);

    domain::collision::onCollision(ship, asteroid);

    EXPECT_EQ(ship.health->current, 80);
    EXPECT_EQ(asteroid.health->current, 60);
    EXPECT_FALSE(asteroid.isDestroyed);
}

TEST(ShipAsteroidCollisionTest, DamageStubDoesNotDestroyLowHealthEntities) {
    auto ship = MakeShip(200.0);
    auto asteroid = MakeAsteroid(100.0);
    ship.health = domain::HealthComponent{1, 1};
    asteroid.health = domain::HealthComponent{1, 1};

    domain::collision::onCollision(ship, asteroid);

    EXPECT_EQ(ship.health->current, 1);
    EXPECT_EQ(asteroid.health->current, 1);
    EXPECT_FALSE(ship.isDestroyed);
    EXPECT_FALSE(asteroid.isDestroyed);
}

TEST(ShipAsteroidCollisionTest, IgnoresInfiniteMasses) {
    auto ship = MakeShip(std::numeric_limits<math::Scalar>::infinity());
    auto asteroid = MakeAsteroid();

    domain::collision::onCollision(ship, asteroid);

    EXPECT_EQ(ship.health->current, 80);
    EXPECT_EQ(asteroid.health->current, 60);
    EXPECT_FALSE(asteroid.isDestroyed);
}

TEST(ShipAsteroidCollisionTest, DoesNothingWhenEntitiesAreNotShipAndAsteroid) {
    auto ship = MakeShip();
    auto asteroid = MakeAsteroid();
    ship.type = types::EntityType::Asteroid;

    domain::collision::onCollision(ship, asteroid);

    EXPECT_EQ(ship.mass, 200.0);
    EXPECT_EQ(ship.health->current, 80);
    EXPECT_EQ(asteroid.health->current, 60);
    EXPECT_FALSE(asteroid.isDestroyed);
}

TEST(ShipAsteroidCollisionTest, CoincidentEntitiesLeaveVelocitiesUnchangedWithRepelStub) {
    auto ship = MakeShip(500.0);
    auto asteroid = MakeAsteroid(100.0);
    ship.transform->velocity = {10.0, 0.0};

    EXPECT_NO_THROW(domain::collision::onCollision(ship, asteroid));

    EXPECT_EQ(ship.transform->velocity, (math::Vector2D{10.0, 0.0}));
    EXPECT_EQ(asteroid.transform->velocity, (math::Vector2D{}));
}

}  // namespace

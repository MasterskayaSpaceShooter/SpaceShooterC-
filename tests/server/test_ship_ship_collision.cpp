#include <gtest/gtest.h>

#include "collision_utils.h"

namespace {

domain::Entity createShip(double mass, std::uint16_t health, std::uint32_t score = 100) {
    domain::Entity ship;
    ship.type = types::EntityType::Ship;
    ship.mass = mass;
    ship.radius = 5.0;
    ship.transform.emplace();
    ship.health.emplace(domain::HealthComponent{health, 100});
    ship.ship.emplace();
    ship.ship->score = score;

    return ship;
}

TEST(ShipShipCollisionTest, LighterShipTakesMoreDamage) {
    auto ship1 = createShip(10.0, 100);
    auto ship2 = createShip(20.0, 100);

    domain::collision::onCollisionShipShip(ship1, ship2);

    EXPECT_EQ(ship1.health->current, 90);
    EXPECT_EQ(ship2.health->current, 95);
}

TEST(ShipShipCollisionTest, EqualMassShipsTakeSevenDamage) {
    auto ship1 = createShip(10.0, 100);
    auto ship2 = createShip(10.0, 100);

    domain::collision::onCollisionShipShip(ship1, ship2);

    EXPECT_EQ(ship1.health->current, 93);
    EXPECT_EQ(ship2.health->current, 93);
}

TEST(ShipShipCollisionTest, BothShipsLoseFiveScorePoints) {
    auto ship1 = createShip(10.0, 100);
    auto ship2 = createShip(20.0, 100);

    domain::collision::onCollisionShipShip(ship1, ship2);

    EXPECT_EQ(ship1.ship->score, 95);
    EXPECT_EQ(ship2.ship->score, 95);
}

TEST(ShipShipCollisionTest, SurvivingShipReceivesRewardAndFrag) {
    auto ship1 = createShip(10.0, 5);
    auto ship2 = createShip(20.0, 100);

    domain::collision::onCollisionShipShip(ship1, ship2);

    EXPECT_TRUE(ship1.isDestroyed);
    EXPECT_FALSE(ship2.isDestroyed);
    EXPECT_EQ(ship2.ship->score, 125);
    EXPECT_EQ(ship2.ship->frags, 1);
}

TEST(ShipShipCollisionTest, DestroyedShipCannotCollideAgain) {
    auto ship1 = createShip(10.0, 100);
    auto ship2 = createShip(20.0, 100);
    ship1.isDestroyed = true;

    domain::collision::onCollisionShipShip(ship1, ship2);

    EXPECT_EQ(ship1.health->current, 100);
    EXPECT_EQ(ship2.health->current, 100);
    EXPECT_EQ(ship1.ship->score, 100);
    EXPECT_EQ(ship2.ship->score, 100);
}

TEST(ShipShipCollisionTest, ScoreCannotBecomeNegative) {
    auto ship1 = createShip(10.0, 100, 3);
    auto ship2 = createShip(20.0, 100, 0);

    domain::collision::onCollisionShipShip(ship1, ship2);

    EXPECT_EQ(ship1.ship->score, 0);
    EXPECT_EQ(ship2.ship->score, 0);
}

}  // namespace

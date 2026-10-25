#include <gtest/gtest.h>

#include "spaceship.h"

namespace {

Spaceship MakeSpaceship() {
    return Spaceship{{10.0, 20.0}, 50.0, 100.0, 75, 100, 5.0, 12, 34};
}

TEST(SpaceshipTest, ConstructorInitializesState) {
    const Spaceship ship = MakeSpaceship();

    EXPECT_EQ(ship.getPosition(), (math::Vector2D{10.0, 20.0}));
    EXPECT_DOUBLE_EQ(ship.getCurrentMass(), 50.0);
    EXPECT_DOUBLE_EQ(ship.getMaxMass(), 100.0);
    EXPECT_EQ(ship.getCurrentHp(), 75);
    EXPECT_EQ(ship.getMaxHp(), 100);
    EXPECT_DOUBLE_EQ(ship.getRadius(), 5.0);
    EXPECT_EQ(ship.getScore(), 12);
    EXPECT_EQ(ship.getXp(), 34);
}

TEST(SpaceshipTest, UpdateMovesByVelocityAndElapsedTime) {
    Spaceship ship = MakeSpaceship();
    ship.setVelocity({3.0, -2.0});

    ship.update(0.5);

    EXPECT_EQ(ship.getPosition(), (math::Vector2D{11.5, 19.0}));
}

TEST(SpaceshipTest, ZeroDeltaDoesNotMove) {
    Spaceship ship = MakeSpaceship();
    ship.setVelocity({3.0, -2.0});

    ship.update(0.0);

    EXPECT_EQ(ship.getPosition(), (math::Vector2D{10.0, 20.0}));
}

TEST(SpaceshipTest, TakeDamageReducesHealth) {
    Spaceship ship = MakeSpaceship();

    ship.takeDamage(25);

    EXPECT_EQ(ship.getCurrentHp(), 50);
}

TEST(SpaceshipTest, TakeDamageDoesNotMakeHealthNegative) {
    Spaceship ship = MakeSpaceship();

    ship.takeDamage(100);

    EXPECT_EQ(ship.getCurrentHp(), 0);
}

TEST(SpaceshipTest, ZeroDamageDoesNotChangeHealth) {
    Spaceship ship = MakeSpaceship();

    ship.takeDamage(0);

    EXPECT_EQ(ship.getCurrentHp(), 75);
}

TEST(SpaceshipTest, AddMassIncreasesMass) {
    Spaceship ship = MakeSpaceship();

    ship.addMass(20.0);

    EXPECT_DOUBLE_EQ(ship.getCurrentMass(), 70.0);
}

TEST(SpaceshipTest, AddMassDoesNotExceedMaximum) {
    Spaceship ship = MakeSpaceship();

    ship.addMass(60.0);

    EXPECT_DOUBLE_EQ(ship.getCurrentMass(), 100.0);
}

TEST(SpaceshipTest, AddMassDoesNotMakeMassNegative) {
    Spaceship ship = MakeSpaceship();

    ship.addMass(-60.0);

    EXPECT_DOUBLE_EQ(ship.getCurrentMass(), 0.0);
}

TEST(SpaceshipTest, UpdatePositionRespawnsAtInitialPositionAndStops) {
    Spaceship ship = MakeSpaceship();
    ship.setPosition({100.0, 200.0});
    ship.setVelocity({3.0, -2.0});

    ship.updatePosition(true);

    EXPECT_EQ(ship.getPosition(), (math::Vector2D{10.0, 20.0}));
    EXPECT_EQ(ship.getVelocity(), (math::Vector2D{}));
}

TEST(SpaceshipTest, UpdatePositionWithoutRespawnPreservesPosition) {
    Spaceship ship = MakeSpaceship();
    ship.setPosition({100.0, 200.0});

    ship.updatePosition(false);

    EXPECT_EQ(ship.getPosition(), (math::Vector2D{100.0, 200.0}));
}

}  // namespace

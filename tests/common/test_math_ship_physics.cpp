#include <gtest/gtest.h>

#include "math_utils.h"

namespace {

using math::kEps;
using math::kPi;
using math::updateShipPhysics;
using math::Vector2D;

TEST(UpdateShipPhysics, ThrustMovesShipAlongXAxis) {
    Vector2D position{0.0, 0.0};
    Vector2D velocity{0.0, 0.0};

    updateShipPhysics(position, velocity, 0.0, true, 100.0, 0.0, 0.1);

    EXPECT_NEAR(velocity.x, 10.0, kEps);
    EXPECT_NEAR(velocity.y, 0.0, kEps);
    EXPECT_NEAR(position.x, 1.0, kEps);
    EXPECT_NEAR(position.y, 0.0, kEps);
}

TEST(UpdateShipPhysics, ThrustMovesShipAlongYAxis) {
    Vector2D position{0.0, 0.0};
    Vector2D velocity{0.0, 0.0};

    updateShipPhysics(position, velocity, kPi / 2.0, true, 100.0, 0.0, 0.1);

    EXPECT_NEAR(velocity.x, 0.0, kEps);
    EXPECT_NEAR(velocity.y, 10.0, kEps);
    EXPECT_NEAR(position.x, 0.0, kEps);
    EXPECT_NEAR(position.y, 1.0, kEps);
}

TEST(UpdateShipPhysics, NoThrustPreservesVelocity) {
    Vector2D position{0.0, 0.0};
    Vector2D velocity{2.0, 3.0};

    updateShipPhysics(position, velocity, 0.0, false, 100.0, 0.0, 0.5);

    EXPECT_NEAR(velocity.x, 2.0, kEps);
    EXPECT_NEAR(velocity.y, 3.0, kEps);
    EXPECT_NEAR(position.x, 1.0, kEps);
    EXPECT_NEAR(position.y, 1.5, kEps);
}

TEST(UpdateShipPhysics, DragReducesVelocity) {
    Vector2D position{0.0, 0.0};
    Vector2D velocity{10.0, 0.0};

    updateShipPhysics(position, velocity, 0.0, false, 0.0, 0.5, 0.2);

    EXPECT_NEAR(velocity.x, 9.0, kEps);
    EXPECT_NEAR(velocity.y, 0.0, kEps);
    EXPECT_NEAR(position.x, 1.8, kEps);
    EXPECT_NEAR(position.y, 0.0, kEps);
}

TEST(UpdateShipPhysics, PositionUsesUpdatedVelocity) {
    Vector2D position{0.0, 0.0};
    Vector2D velocity{2.0, 0.0};

    updateShipPhysics(position, velocity, 0.0, true, 10.0, 0.0, 0.1);

    EXPECT_NEAR(velocity.x, 3.0, kEps);
    EXPECT_NEAR(position.x, 0.3, kEps);
}

TEST(UpdateShipPhysics, ZeroDtDoesNotChangeState) {
    Vector2D position{4.0, 5.0};
    Vector2D velocity{2.0, 3.0};

    updateShipPhysics(position, velocity, 0.0, true, 100.0, 0.5, 0.0);

    EXPECT_NEAR(velocity.x, 2.0, kEps);
    EXPECT_NEAR(velocity.y, 3.0, kEps);
    EXPECT_NEAR(position.x, 4.0, kEps);
    EXPECT_NEAR(position.y, 5.0, kEps);
}

TEST(UpdateShipPhysics, ZeroThrustPowerDoesNotAccelerateShip) {
    Vector2D position{0.0, 0.0};
    Vector2D velocity{1.0, 2.0};

    updateShipPhysics(position, velocity, kPi / 4.0, true, 0.0, 0.0, 0.5);

    EXPECT_NEAR(velocity.x, 1.0, kEps);
    EXPECT_NEAR(velocity.y, 2.0, kEps);
    EXPECT_NEAR(position.x, 0.5, kEps);
    EXPECT_NEAR(position.y, 1.0, kEps);
}

TEST(UpdateShipPhysics, DragIsAppliedAfterThrust) {
    Vector2D position{0.0, 0.0};
    Vector2D velocity{0.0, 0.0};

    updateShipPhysics(position, velocity, 0.0, true, 100.0, 0.5, 0.1);

    EXPECT_NEAR(velocity.x, 9.5, kEps);
    EXPECT_NEAR(velocity.y, 0.0, kEps);
    EXPECT_NEAR(position.x, 0.95, kEps);
    EXPECT_NEAR(position.y, 0.0, kEps);
}

TEST(UpdateShipPhysics, DragTimesDtEqualsOneStopsShip) {
    Vector2D position{0.0, 0.0};
    Vector2D velocity{10.0, 5.0};

    updateShipPhysics(position, velocity, 0.0, false, 0.0, 0.5, 2.0);

    EXPECT_NEAR(velocity.x, 0.0, kEps);
    EXPECT_NEAR(velocity.y, 0.0, kEps);
    EXPECT_NEAR(position.x, 0.0, kEps);
    EXPECT_NEAR(position.y, 0.0, kEps);
}

TEST(UpdateShipPhysics, DragTimesDtAboveOneStopsShip) {
    Vector2D position{0.0, 0.0};
    Vector2D velocity{10.0, 5.0};

    updateShipPhysics(position, velocity, 0.0, false, 0.0, 0.5, 3.0);

    EXPECT_NEAR(velocity.x, 0.0, kEps);
    EXPECT_NEAR(velocity.y, 0.0, kEps);
    EXPECT_NEAR(position.x, 0.0, kEps);
    EXPECT_NEAR(position.y, 0.0, kEps);
}

TEST(UpdateShipPhysics, ThrustThenStrongDragStopsShip) {
    Vector2D position{0.0, 0.0};
    Vector2D velocity{0.0, 0.0};

    updateShipPhysics(position, velocity, 0.0, true, 100.0, 0.5, 3.0);

    EXPECT_NEAR(velocity.x, 0.0, kEps);
    EXPECT_NEAR(velocity.y, 0.0, kEps);
    EXPECT_NEAR(position.x, 0.0, kEps);
    EXPECT_NEAR(position.y, 0.0, kEps);
}

}  // namespace

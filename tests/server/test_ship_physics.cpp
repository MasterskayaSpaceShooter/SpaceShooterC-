#include <cmath>
#include <gtest/gtest.h>

#include "ship_physics.h"

namespace {

using domain::TransformComponent;
using domain::ship_physics::stepShip;
using math::Scalar;
using math::Vector2D;

constexpr Scalar kDt = 1.0 / 60.0;
constexpr Scalar kEps = 1e-3;

TransformComponent makeTransform(Vector2D pos, Vector2D vel, Scalar angle) {
    TransformComponent t;
    t.position = pos;
    t.velocity = vel;
    t.angle = angle;
    return t;
}

}  // namespace

// Разгон из состояния покоя (вправо по оси X).
TEST(ShipPhysics, AcceleratesFromRest) {
    auto t = makeTransform({0.0, 0.0}, {0.0, 0.0}, 0.0);

    stepShip(t, {1.0, 0.0}, kDt);

    EXPECT_NEAR(t.velocity.x, 8.125, kEps);
    EXPECT_NEAR(t.velocity.y, 0.0, kEps);
    EXPECT_NEAR(t.position.x, 0.1354, kEps);
    EXPECT_NEAR(t.position.y, 0.0, kEps);
    EXPECT_NEAR(t.angle, 0.0, kEps);
}

// Превышение лимита скорости (max_speed).
TEST(ShipPhysics, ClampsToMaxSpeed) {
    auto t = makeTransform({100.0, 200.0}, {300.0, 0.0}, 0.0);

    stepShip(t, {1.0, 0.0}, kDt);

    EXPECT_NEAR(t.velocity.x, 292.5, kEps);
    EXPECT_NEAR(t.velocity.y, 0.0, kEps);
    EXPECT_NEAR(t.position.x, 104.875, kEps);
    EXPECT_NEAR(t.position.y, 200.0, kEps);
    EXPECT_NEAR(t.angle, 0.0, kEps);
}

// Замедление по инерции при отсутствии тяги (d = 0).
TEST(ShipPhysics, DeceleratesByDragWithoutThrust) {
    auto t = makeTransform({50.0, 50.0}, {50.0, -50.0}, -0.7854);

    stepShip(t, {0.0, 0.0}, kDt);

    EXPECT_NEAR(t.velocity.x, 48.75, kEps);
    EXPECT_NEAR(t.velocity.y, -48.75, kEps);
    EXPECT_NEAR(t.position.x, 50.8125, kEps);
    EXPECT_NEAR(t.position.y, 49.1875, kEps);
    EXPECT_NEAR(t.angle, -0.7854, kEps);
}

// Остановка при падении скорости ниже порога (stop_threshold).
TEST(ShipPhysics, StopsBelowThreshold) {
    auto t = makeTransform({10.0, 10.0}, {0.08, 0.0}, 1.5708);

    stepShip(t, {0.0, 0.0}, kDt);

    EXPECT_NEAR(t.velocity.x, 0.0, kEps);
    EXPECT_NEAR(t.velocity.y, 0.0, kEps);
    EXPECT_NEAR(t.position.x, 10.0, kEps);
    EXPECT_NEAR(t.position.y, 10.0, kEps);
    EXPECT_NEAR(t.angle, 1.5708, kEps);
}

TEST(ShipPhysics, ZeroDtIsNoOp) {
    auto t = makeTransform({1.0, 2.0}, {3.0, 4.0}, 0.5);
    const auto before = t;

    stepShip(t, {1.0, 0.0}, 0.0);

    EXPECT_DOUBLE_EQ(t.position.x, before.position.x);
    EXPECT_DOUBLE_EQ(t.position.y, before.position.y);
    EXPECT_DOUBLE_EQ(t.velocity.x, before.velocity.x);
    EXPECT_DOUBLE_EQ(t.velocity.y, before.velocity.y);
    EXPECT_DOUBLE_EQ(t.angle, before.angle);
}

TEST(ShipPhysics, NegativeDtIsNoOp) {
    auto t = makeTransform({1.0, 2.0}, {3.0, 4.0}, 0.5);
    const auto before = t;

    stepShip(t, {1.0, 0.0}, -kDt);

    EXPECT_DOUBLE_EQ(t.position.x, before.position.x);
    EXPECT_DOUBLE_EQ(t.position.y, before.position.y);
    EXPECT_DOUBLE_EQ(t.velocity.x, before.velocity.x);
    EXPECT_DOUBLE_EQ(t.velocity.y, before.velocity.y);
    EXPECT_DOUBLE_EQ(t.angle, before.angle);
}

TEST(ShipPhysics, PartialInputIsNotNormalized) {
    auto t = makeTransform({0.0, 0.0}, {0.0, 0.0}, 0.0);

    stepShip(t, {0.5, 0.0}, kDt);  // |d| = 0.5

    // v = 0.5 * 500 * (1/60) * 0.975 = 0.5 * 8.125 = 4.0625
    EXPECT_NEAR(t.velocity.x, 4.0625, kEps);
    EXPECT_NEAR(t.velocity.y, 0.0, kEps);
}

TEST(ShipPhysics, DiagonalInputIsNormalized) {
    auto t = makeTransform({0.0, 0.0}, {0.0, 0.0}, 0.0);

    stepShip(t, {1.0, 1.0}, kDt);  // |d| = sqrt(2) > 1

    const auto len = t.velocity.length();
    EXPECT_NEAR(len, 8.125, kEps);
    EXPECT_NEAR(t.velocity.x, t.velocity.y, kEps);
}

TEST(ShipPhysics, OversizedInputIsClampedToUnit) {
    auto t = makeTransform({0.0, 0.0}, {0.0, 0.0}, 0.0);

    stepShip(t, {2.0, 0.0}, kDt);  // |d| = 2 > 1

    EXPECT_NEAR(t.velocity.x, 8.125, kEps);
    EXPECT_NEAR(t.velocity.y, 0.0, kEps);
}

TEST(ShipPhysics, VelocityExactlyAtThresholdIsKept) {
    // Подбираем v0 так, чтобы после drag |v| == stop_threshold == 0.1:
    // v_drag = v0 * (1 - kLinearDrag*dt) = v0 * 0.975 = 0.1
    const Scalar v0 = 0.1 / (1.0 - 1.5 * kDt);
    auto t = makeTransform({0.0, 0.0}, {v0, 0.0}, 0.0);

    stepShip(t, {0.0, 0.0}, kDt);

    EXPECT_NEAR(t.velocity.x, 0.1, kEps);
    EXPECT_GT(t.velocity.x, 0.0);
}

TEST(ShipPhysics, SlightlyAboveThresholdUpdatesAngle) {
    auto t = makeTransform({0.0, 0.0}, {0.0, 0.2}, 0.0);

    stepShip(t, {0.0, 0.0}, kDt);

    EXPECT_NEAR(t.velocity.y, 0.195, kEps);
    EXPECT_NEAR(t.angle, math::kPi / 2, kEps);
}

TEST(ShipPhysics, AngleIsPreservedWhenStopped) {
    auto t = makeTransform({0.0, 0.0}, {0.05, 0.0}, 1.23);

    stepShip(t, {0.0, 0.0}, kDt);

    EXPECT_DOUBLE_EQ(t.velocity.x, 0.0);
    EXPECT_DOUBLE_EQ(t.velocity.y, 0.0);
    EXPECT_DOUBLE_EQ(t.angle, 1.23);
}

#include <gtest/gtest.h>
#include <math_utils.h>
#include <stdexcept>

namespace {

using math::Vector2D;
using math::collision::checkCircleCollision;

TEST(CheckCircleCollision, ReturnsTrueWhenCirclesIntersect) {
    const Vector2D pos1(0, 0);
    const Vector2D pos2(15, 0);

    EXPECT_TRUE(checkCircleCollision(pos1, 10, pos2, 10));
}

TEST(CheckCircleCollision, ReturnsFalseWhenCirclesDoNotIntersect) {
    const Vector2D pos1(0, 0);
    const Vector2D pos2(30, 0);

    EXPECT_FALSE(checkCircleCollision(pos1, 10, pos2, 10));
}

TEST(CheckCircleCollision, ReturnsTrueWhenCirclesIntersectAtNegativeCoordinates) {
    const Vector2D pos1(-15, -20);
    const Vector2D pos2(-5, -20);

    EXPECT_TRUE(checkCircleCollision(pos1, 6, pos2, 6));
}

TEST(CheckCircleCollision, ReturnsTrueWhenCirclesAreConcentric) {
    const Vector2D pos(-3, 8);

    EXPECT_TRUE(checkCircleCollision(pos, 2, pos, 5));
}

TEST(CheckCircleCollision, ThrowsWhenFirstRadiusIsNegative) {
    const Vector2D pos1(0, 0);
    const Vector2D pos2(1, 0);

    EXPECT_THROW(static_cast<void>(checkCircleCollision(pos1, -1, pos2, 1)), std::invalid_argument);
}

TEST(CheckCircleCollision, ThrowsWhenSecondRadiusIsNegative) {
    const Vector2D pos1(0, 0);
    const Vector2D pos2(1, 0);

    EXPECT_THROW(static_cast<void>(checkCircleCollision(pos1, 1, pos2, -1)), std::invalid_argument);
}

}  // namespace

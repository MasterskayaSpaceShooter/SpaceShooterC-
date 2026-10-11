#include <gtest/gtest.h>
#include <math_utils.h>

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

}  // namespace

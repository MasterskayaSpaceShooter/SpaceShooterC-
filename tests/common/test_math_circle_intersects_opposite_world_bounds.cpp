#include <gtest/gtest.h>
#include <limits>

#include "math_utils.h"

using math::circleIntersectsOppositeWorldBounds;
using math::Scalar;
using math::Vector2D;

TEST(CircleIntersectsOppositeWorldBounds, TangencyToBothOpposingEdgesReturnsTrue) {
    EXPECT_TRUE(circleIntersectsOppositeWorldBounds(Vector2D{0.0, 0.0}, 10.0, 20.0, 40.0));
    EXPECT_TRUE(circleIntersectsOppositeWorldBounds(Vector2D{0.0, 1.0}, 8.0, 40.0, 12.0));
}

TEST(CircleIntersectsOppositeWorldBounds, CrossingBothOpposingEdgesReturnsTrue) {
    EXPECT_TRUE(circleIntersectsOppositeWorldBounds(Vector2D{0.0, 0.0}, 12.0, 20.0, 40.0));
}

TEST(CircleIntersectsOppositeWorldBounds, IntersectingOnlyOneEdgeReturnsFalse) {
    EXPECT_FALSE(circleIntersectsOppositeWorldBounds(Vector2D{4.0, 0.0}, 10.0, 20.0, 40.0));
}

TEST(CircleIntersectsOppositeWorldBounds, CircleInsideWithoutTouchingOpposingEdgesReturnsFalse) {
    EXPECT_FALSE(circleIntersectsOppositeWorldBounds(Vector2D{0.0, 0.0}, 9.0, 20.0, 40.0));
}

TEST(CircleIntersectsOppositeWorldBounds, DoesNotCountIntersectionsOutsideFiniteEdgeSegments) {
    EXPECT_FALSE(circleIntersectsOppositeWorldBounds(Vector2D{0.0, 20.0}, 10.0, 20.0, 10.0));
}

TEST(CircleIntersectsOppositeWorldBounds, ReturnsFalseForInvalidInputs) {
    constexpr Scalar world_size = 20.0;
    EXPECT_FALSE(circleIntersectsOppositeWorldBounds(Vector2D{}, -1.0, world_size, world_size));
    EXPECT_FALSE(circleIntersectsOppositeWorldBounds(Vector2D{}, 1.0, 0.0, world_size));
    EXPECT_FALSE(circleIntersectsOppositeWorldBounds(Vector2D{std::numeric_limits<Scalar>::infinity(), 0.0},
                                                     1.0,
                                                     world_size,
                                                     world_size));
}

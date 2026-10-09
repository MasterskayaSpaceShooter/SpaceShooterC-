#include <gtest/gtest.h>

#include "math_utils.h"

namespace {

using math::Scalar;
using math::Vector2D;

TEST(Vector2DTest, DefaultCtorIsZero) {
    const Vector2D v;
    EXPECT_DOUBLE_EQ(v.x, 0.0);
    EXPECT_DOUBLE_EQ(v.y, 0.0);
}

TEST(Vector2DTest, CtorStoresCoordinates) {
    const Vector2D v(2.5, -3.0);
    EXPECT_DOUBLE_EQ(v.x, 2.5);
    EXPECT_DOUBLE_EQ(v.y, -3.0);
}

TEST(Vector2DTest, AddAssign) {
    Vector2D v(1.0, 2.0);
    auto& ref = (v += Vector2D(3.0, -1.0));
    EXPECT_EQ(&ref, &v);  // должен возвращать *this
    EXPECT_DOUBLE_EQ(v.x, 4.0);
    EXPECT_DOUBLE_EQ(v.y, 1.0);
}

TEST(Vector2DTest, SubAssign) {
    Vector2D v(5.0, 4.0);
    auto& ref = (v -= Vector2D(2.0, 6.0));
    EXPECT_EQ(&ref, &v);
    EXPECT_DOUBLE_EQ(v.x, 3.0);
    EXPECT_DOUBLE_EQ(v.y, -2.0);
}

TEST(Vector2DTest, MulAssignScalar) {
    Vector2D v(2.0, -3.0);
    auto& ref = (v *= 2.5);
    EXPECT_EQ(&ref, &v);
    EXPECT_DOUBLE_EQ(v.x, 5.0);
    EXPECT_DOUBLE_EQ(v.y, -7.5);
}

TEST(Vector2DTest, AddOperator) {
    const Vector2D a(1.0, 2.0);
    const Vector2D b(3.0, 4.0);
    const Vector2D r = a + b;
    EXPECT_DOUBLE_EQ(r.x, 4.0);
    EXPECT_DOUBLE_EQ(r.y, 6.0);
    // Операнды не должны меняться
    EXPECT_DOUBLE_EQ(a.x, 1.0);
    EXPECT_DOUBLE_EQ(a.y, 2.0);
    EXPECT_DOUBLE_EQ(b.x, 3.0);
    EXPECT_DOUBLE_EQ(b.y, 4.0);
}

TEST(Vector2DTest, SubOperator) {
    const Vector2D a(5.0, 7.0);
    const Vector2D b(2.0, 10.0);
    const Vector2D r = a - b;
    EXPECT_DOUBLE_EQ(r.x, 3.0);
    EXPECT_DOUBLE_EQ(r.y, -3.0);
}

TEST(Vector2DTest, ScalarMultiplication) {
    const Vector2D v(2.0, -4.0);
    const Vector2D left = v * 3.0;
    const Vector2D right = 3.0 * v;
    EXPECT_DOUBLE_EQ(left.x, 6.0);
    EXPECT_DOUBLE_EQ(left.y, -12.0);
    EXPECT_DOUBLE_EQ(right.x, 6.0);
    EXPECT_DOUBLE_EQ(right.y, -12.0);
}

TEST(Vector2DTest, Equality) {
    const Vector2D a(1.0, 2.0);
    const Vector2D b(1.0, 2.0);
    const Vector2D c(1.0, 3.0);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_FALSE(a != b);
    EXPECT_TRUE(a != c);

    Scalar x = 0.2 + 0.1;
    const Vector2D d(0, x);
    const Vector2D e(0, 0.3);
    EXPECT_TRUE(d == e);
}

TEST(Vector2DTest, LengthSquared) {
    const Vector2D a(3.0, 4.0);
    EXPECT_DOUBLE_EQ(a.lengthSquared(), 25.0);
    const Vector2D unit(1.0, 0.0);
    EXPECT_DOUBLE_EQ(unit.lengthSquared(), 1.0);
    const Vector2D zero(0.0, 0.0);
    EXPECT_DOUBLE_EQ(zero.lengthSquared(), 0.0);
}

TEST(Vector2DTest, Length) {
    const Vector2D a(3.0, 4.0);
    EXPECT_DOUBLE_EQ(a.length(), 5.0);
    const Vector2D negative(-3.0, -4.0);
    EXPECT_DOUBLE_EQ(negative.length(), 5.0);
    const Vector2D zero(0.0, 0.0);
    EXPECT_DOUBLE_EQ(zero.length(), 0.0);
}

TEST(Vector2DTest, NormalizedUpVector) {
    const Vector2D v(0.0, 10.0);
    const Vector2D n = v.normalized();
    EXPECT_DOUBLE_EQ(n.x, 0.0);
    EXPECT_DOUBLE_EQ(n.y, 1.0);
}

TEST(Vector2DTest, NormalizedNegativeCoordinates) {
    const Vector2D v(0.0, -10.0);
    const Vector2D n = v.normalized();
    EXPECT_DOUBLE_EQ(n.x, 0.0);
    EXPECT_DOUBLE_EQ(n.y, -1.0);
}

TEST(Vector2DTest, NormalizedGenericVector) {
    const Vector2D v(3.0, 4.0);
    const Vector2D n = v.normalized();
    EXPECT_DOUBLE_EQ(n.x, 0.6);
    EXPECT_DOUBLE_EQ(n.y, 0.8);
    EXPECT_DOUBLE_EQ(n.length(), 1.0);
    // Исходный вектор не мутируется
    EXPECT_DOUBLE_EQ(v.x, 3.0);
    EXPECT_DOUBLE_EQ(v.y, 4.0);
}

TEST(Vector2DTest, NormalizedZeroVector) {
    const Vector2D zero(0.0, 0.0);
    const Vector2D n = zero.normalized();
    EXPECT_DOUBLE_EQ(n.x, 0.0);
    EXPECT_DOUBLE_EQ(n.y, 0.0);
}

}  // namespace

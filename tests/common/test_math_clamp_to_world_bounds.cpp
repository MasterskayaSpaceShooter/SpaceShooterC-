#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

#include "math_utils.h"

using math::clampToWorldBounds;
using math::kEps;
using math::Scalar;
using math::Vector2D;

namespace {

// позиция внутри мира не меняется
TEST(ClampToWorldBounds, InsideWorldUnchanged) {
    constexpr Scalar world_width = 800.0;
    constexpr Scalar world_height = 600.0;
    const Vector2D pos(100.0, -200.0);
    const Vector2D result = clampToWorldBounds(pos, world_width, world_height);
    EXPECT_DOUBLE_EQ(result.x, 100.0);
    EXPECT_DOUBLE_EQ(result.y, -200.0);
}

// точка ровно на краю мира притягивается на kEps внутрь,
// чтобы оставаться строго в границах
TEST(ClampToWorldBounds, OnTrueBoundaryClampedInward) {
    constexpr Scalar world_width = 800.0;
    constexpr Scalar world_height = 600.0;
    const Vector2D pos(world_width * 0.5, -world_height * 0.5);
    const Vector2D result = clampToWorldBounds(pos, world_width, world_height);
    EXPECT_DOUBLE_EQ(result.x, world_width * 0.5 - kEps);
    EXPECT_DOUBLE_EQ(result.y, -world_height * 0.5 + kEps);
}

// выход за каждую из четырёх границ сжимается к соответствующей полугранице
TEST(ClampToWorldBounds, BeyondEdgesClamped) {
    constexpr Scalar world_width = 800.0;
    constexpr Scalar world_height = 600.0;
    constexpr Scalar half_w = world_width * 0.5;
    constexpr Scalar half_h = world_height * 0.5;

    const Vector2D left = clampToWorldBounds(Vector2D(-world_width, 0.0), world_width, world_height);
    EXPECT_DOUBLE_EQ(left.x, -half_w + kEps);

    const Vector2D right = clampToWorldBounds(Vector2D(world_width, 0.0), world_width, world_height);
    EXPECT_DOUBLE_EQ(right.x, half_w - kEps);

    const Vector2D bottom = clampToWorldBounds(Vector2D(0.0, -world_height), world_width, world_height);
    EXPECT_DOUBLE_EQ(bottom.y, -half_h + kEps);

    const Vector2D top = clampToWorldBounds(Vector2D(0.0, world_height), world_width, world_height);
    EXPECT_DOUBLE_EQ(top.y, half_h - kEps);
}

// угол за обеими границами сжимается по каждой оси независимо,
// результат — отодвинутая на kEps внутрь граница
TEST(ClampToWorldBounds, CornerClampsBothAxes) {
    constexpr Scalar world_width = 800.0;
    constexpr Scalar world_height = 600.0;
    const Vector2D pos(-2.0 * world_width, 3.0 * world_height);
    const Vector2D result = clampToWorldBounds(pos, world_width, world_height);
    EXPECT_DOUBLE_EQ(result.x, -world_width * 0.5 + kEps);
    EXPECT_DOUBLE_EQ(result.y, world_height * 0.5 - kEps);
}

// точка чуть внутри края мира (в пределах kEps от него) кламперуется
// на границу, отодвинутую на kEps внутрь
TEST(ClampToWorldBounds, JustInsideTrueBoundaryClampedToInset) {
    constexpr Scalar world_width = 800.0;
    constexpr Scalar world_height = 600.0;
    constexpr Scalar half_w = world_width * 0.5;
    constexpr Scalar half_h = world_height * 0.5;

    const Vector2D right =
        clampToWorldBounds(Vector2D(half_w - 0.5 * kEps, -half_h + 0.5 * kEps), world_width, world_height);
    EXPECT_DOUBLE_EQ(right.x, half_w - kEps);
    EXPECT_DOUBLE_EQ(right.y, -half_h + kEps);
}

// точка чуть снаружи края мира (в пределах kEps) кламперуется
// на ту же отодвинутую внутрь границу
TEST(ClampToWorldBounds, JustOutsideTrueBoundaryClampedToInset) {
    constexpr Scalar world_width = 800.0;
    constexpr Scalar world_height = 600.0;
    constexpr Scalar half_w = world_width * 0.5;
    constexpr Scalar half_h = world_height * 0.5;

    const Vector2D right =
        clampToWorldBounds(Vector2D(half_w + 0.5 * kEps, -half_h - 0.5 * kEps), world_width, world_height);
    EXPECT_DOUBLE_EQ(right.x, half_w - kEps);
    EXPECT_DOUBLE_EQ(right.y, -half_h + kEps);
}

// точка внутри, но дальше чем kEps от границы, не меняется
TEST(ClampToWorldBounds, OutsideEpsilonZoneUnchanged) {
    constexpr Scalar world_width = 800.0;
    constexpr Scalar world_height = 600.0;
    constexpr Scalar half_w = world_width * 0.5;
    constexpr Scalar half_h = world_height * 0.5;

    const Vector2D pos(half_w - 2 * kEps, -half_h + 2 * kEps);
    const Vector2D result = clampToWorldBounds(pos, world_width, world_height);
    EXPECT_DOUBLE_EQ(result.x, pos.x);
    EXPECT_DOUBLE_EQ(result.y, pos.y);
}

// конкретный сценарий: мир 2000x2000, точка за обеими границами —
// результат лежит на kEps внутри от краёв мира
TEST(ClampToWorldBounds, GivenCoordinatesClampedToWorldSize) {
    constexpr Scalar world_width = 2000.0;
    constexpr Scalar world_height = 2000.0;
    const Vector2D pos(1500.0, -1200.0);
    const Vector2D result = clampToWorldBounds(pos, world_width, world_height);
    EXPECT_DOUBLE_EQ(result.x, 1000.0 - kEps);
    EXPECT_DOUBLE_EQ(result.y, -1000.0 + kEps);
}

namespace detail {

// compile-time детектор: value == false, если вызов clampToWorldBounds с
// типами T1/T2 невозможен на этапе компиляции (deleted-перегрузка).
template <typename T1, typename T2, typename = void>
struct IsCallableWith : std::false_type {};

template <typename T1, typename T2>
struct IsCallableWith<
    T1,
    T2,
    std::void_t<decltype(clampToWorldBounds(std::declval<Vector2D>(), std::declval<T1>(), std::declval<T2>()))>>
    : std::true_type {};

}  // namespace detail

// аргументы ширины и высоты принимаются строго как double:
// int/float и смешанные типы запрещены на этапе компиляции
static_assert(detail::IsCallableWith<double, double>::value);
static_assert(!detail::IsCallableWith<int, int>::value);
static_assert(!detail::IsCallableWith<float, float>::value);
static_assert(!detail::IsCallableWith<int, double>::value);

}  // namespace

#include <gtest/gtest.h>
#include <utility>
#include <vector>

#include "math_utils.h"

namespace {

using math::Circle;
using math::largestEmptyCircle;
using math::Scalar;
using math::Vector2D;

constexpr Scalar kTol = 1e-9;

// Вспомогательная проверка допустимости окружности: она внутри мира
// и не пересекается/не касается ни одного занятого круга.
void expectValidCircle(const Circle& c,
                       const std::vector<std::pair<Vector2D, Scalar>>& occupied,
                       Scalar world_width,
                       Scalar world_height) {
    const Scalar half_w = world_width * 0.5;
    const Scalar half_h = world_height * 0.5;

    EXPECT_GE(c.radius, 0.0);

    // внутри мира
    EXPECT_LE(std::fabs(c.center.x) + c.radius, half_w + kTol);
    EXPECT_LE(std::fabs(c.center.y) + c.radius, half_h + kTol);

    // не касается и не пересекает занятые круги
    for (const auto& [pos, rad] : occupied) {
        EXPECT_GT((c.center - pos).length(), c.radius + rad - kTol);
    }
}

// Пустой мир: наибольшая свободная окружность — вписанная в мир.
TEST(LargestEmptyCircle, EmptyWorldReturnsInscribedCircle) {
    const Scalar w = 800.0;
    const Scalar h = 600.0;
    const std::vector<std::pair<Vector2D, Scalar>> occupied;

    const Circle c = largestEmptyCircle(occupied, w, h);

    EXPECT_DOUBLE_EQ(c.center.x, 0.0);
    EXPECT_DOUBLE_EQ(c.center.y, 0.0);
    EXPECT_DOUBLE_EQ(c.radius, 300.0);  // min(800/2, 600/2)
}

// Один круг в центре квадратного мира: наибольшая свободная окружность
// занимает пространство между кругом и ближайшей границей мира.
TEST(LargestEmptyCircle, SingleCircleAtCenter) {
    const Scalar w = 1000.0;
    const Scalar h = 1000.0;
    const std::vector<std::pair<Vector2D, Scalar>> occupied{{Vector2D{0.0, 0.0}, 100.0}};

    const Circle c = largestEmptyCircle(occupied, w, h);

    expectValidCircle(c, occupied, w, h);

    // Круг в центре мира: окружность касается круга и границы на одной линии.
    // R = (half_w - r) / 2 = (500 - 100) / 2 = 200.
    EXPECT_NEAR(c.radius, 200.0, 1e-3);
}

// Два одинаковых круга по горизонтали: крупнейшая пустая окружность —
// в вертикальном зазоре между кругом и границей мира (не между кругами).
TEST(LargestEmptyCircle, TwoCirclesSymmetrical) {
    const Scalar w = 1000.0;
    const Scalar h = 1000.0;
    const std::vector<std::pair<Vector2D, Scalar>> occupied{
        {Vector2D{-200.0, 0.0}, 50.0},
        {Vector2D{200.0, 0.0}, 50.0},
    };

    const Circle c = largestEmptyCircle(occupied, w, h);

    expectValidCircle(c, occupied, w, h);

    // Пространство между кругами даёт R=150, но больше места сверху/снизу:
    // зазор от круга до верхней границы = 500, поэтому R = (500 - 50) / 2 = 225.
    EXPECT_NEAR(c.radius, 225.0, 1e-3);
}

// Круг у левого края: максимум заполняет свободное пространство справа.
TEST(LargestEmptyCircle, SingleCircleNearLeftEdge) {
    const Scalar w = 1000.0;
    const Scalar h = 1000.0;
    const std::vector<std::pair<Vector2D, Scalar>> occupied{{Vector2D{-400.0, 0.0}, 50.0}};

    const Circle c = largestEmptyCircle(occupied, w, h);

    expectValidCircle(c, occupied, w, h);

    // Доминирует правое ребро: R = (half_w - px - r) / 2 = (500 + 400 - 50) / 2 = 425.
    EXPECT_GT(c.radius, 420.0);
    EXPECT_LT(c.radius, 426.0);
}

// Перекрывающиеся (вложенные) круги: меньший не уменьшает результат.
TEST(LargestEmptyCircle, NestedCirclesAreMergedInEffect) {
    const Scalar w = 1000.0;
    const Scalar h = 1000.0;
    const std::vector<std::pair<Vector2D, Scalar>> occupied{
        {Vector2D{0.0, 0.0}, 50.0},
        {Vector2D{0.0, 0.0}, 10.0},
    };

    const Circle c = largestEmptyCircle(occupied, w, h);

    expectValidCircle(c, occupied, w, h);

    // Доминирует внешний круг r=50; результат такой же, как с одним кругом r=50.
    const Circle single = largestEmptyCircle({{Vector2D{0.0, 0.0}, 50.0}}, w, h);
    EXPECT_NEAR(c.radius, single.radius, kTol);
}

// Круги в углах: максимум — в свободном центре мира.
TEST(LargestEmptyCircle, CirclesInCornersCenterIsFree) {
    const Scalar w = 1000.0;
    const Scalar h = 1000.0;
    const std::vector<std::pair<Vector2D, Scalar>> occupied{
        {Vector2D{-400.0, -400.0}, 50.0},
        {Vector2D{400.0, -400.0}, 50.0},
        {Vector2D{-400.0, 400.0}, 50.0},
        {Vector2D{400.0, 400.0}, 50.0},
    };

    const Circle c = largestEmptyCircle(occupied, w, h);

    expectValidCircle(c, occupied, w, h);

    EXPECT_NEAR(c.center.x, 0.0, 1e-6);
    EXPECT_NEAR(c.center.y, 0.0, 1e-6);
    EXPECT_GT(c.radius, 200.0);
}

}  // namespace

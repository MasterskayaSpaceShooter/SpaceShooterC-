#include <gtest/gtest.h>

#include "math_utils.h"

using math::kEps;
using math::kPi;
using math::normalizeAngle;
using math::Scalar;

// вспомогательная функция
// проверяет что результат лежит в диапазоне (-pi, pi]
bool isRange(Scalar a) {
    return (-kPi < a && a <= kPi);
}

// уже нормализованные угла
TEST(NormalizeAngle, AlreadyNormalize) {
    EXPECT_NEAR(normalizeAngle(0.0), 0.0, kEps);
    EXPECT_NEAR(normalizeAngle(kPi), kPi, kEps);
    EXPECT_NEAR(normalizeAngle(-kPi), kPi, kEps);

    // углы в разный четвертях окружности
    EXPECT_NEAR(normalizeAngle(kPi / 4), kPi / 4, kEps);            // I
    EXPECT_NEAR(normalizeAngle(3 * kPi / 4), 3 * kPi / 4, kEps);    // II
    EXPECT_NEAR(normalizeAngle(-3 * kPi / 4), -3 * kPi / 4, kEps);  // III
    EXPECT_NEAR(normalizeAngle(-kPi / 4), -kPi / 4, kEps);          // IV

    // половинки kPi
    EXPECT_NEAR(normalizeAngle(kPi / 2), kPi / 2, kEps);
    EXPECT_NEAR(normalizeAngle(-kPi / 2), -kPi / 2, kEps);
}

// границы (-pi, pi]
TEST(NormalizeAngle, BoundariesNormalize) {
    EXPECT_NEAR(normalizeAngle(kPi), kPi, kEps);
    EXPECT_NEAR(normalizeAngle(-kPi), kPi, kEps);
    EXPECT_NEAR(normalizeAngle(-3 * kPi), kPi, kEps);
    EXPECT_NEAR(normalizeAngle(3 * kPi), kPi, kEps);
}

// большие и маленькие значения
TEST(NormalizeAngle, LargeAndSmallNumberNormalize) {
    EXPECT_TRUE(isRange(normalizeAngle(1e6)));
    EXPECT_TRUE(isRange(normalizeAngle(-1e6)));
    EXPECT_TRUE(isRange(normalizeAngle(1e-6)));
    EXPECT_TRUE(isRange(normalizeAngle(-1e-6)));
}

// близкие числа к граничным
TEST(NormalizeAngle, Normalize) {
    EXPECT_NEAR(normalizeAngle(-kPi + 1e-15), -kPi + 1e-15, kEps);
}

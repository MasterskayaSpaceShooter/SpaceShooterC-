#include <gtest/gtest.h>

#include "math_utils.h"

using math::kPi;
using math::normalizeAngle;
using math::Scalar;

Scalar eps = 1e-6;
// вспомогательная функция
// проверяет что результат лежит в диапазоне (-pi, pi]
bool isRange(Scalar a) {
    return (-kPi < a && a <= kPi);
}

// уже нормализованные угла
TEST(NormalizeAngle, AlreadyNormalize) {
    EXPECT_NEAR(normalizeAngle(0.0), 0.0, eps);
    EXPECT_NEAR(normalizeAngle(kPi), kPi, eps);
    EXPECT_NEAR(normalizeAngle(-kPi), kPi, eps);

    // углы в разный четвертях окружности
    EXPECT_NEAR(normalizeAngle(kPi / 4), kPi / 4, eps);            // I
    EXPECT_NEAR(normalizeAngle(3 * kPi / 4), 3 * kPi / 4, eps);    // II
    EXPECT_NEAR(normalizeAngle(-3 * kPi / 4), -3 * kPi / 4, eps);  // III
    EXPECT_NEAR(normalizeAngle(-kPi / 4), -kPi / 4, eps);          // IV

    // половинки kPi
    EXPECT_NEAR(normalizeAngle(kPi / 2), kPi / 2, eps);
    EXPECT_NEAR(normalizeAngle(-kPi / 2), -kPi / 2, eps);
}

// границы (-pi, pi]
TEST(NormalizeAngle, BoundariesNormalize) {
    EXPECT_NEAR(normalizeAngle(kPi), kPi, eps);
    EXPECT_NEAR(normalizeAngle(-kPi), kPi, eps);
    EXPECT_NEAR(normalizeAngle(-3 * kPi), kPi, eps);
    EXPECT_NEAR(normalizeAngle(3 * kPi), kPi, eps);
}

// большие и маленькие значения
TEST(NormalizeAngle, LargeAndSmallNumberNormalize) {
    EXPECT_TRUE(isRange(normalizeAngle(1e11)));
    EXPECT_TRUE(isRange(normalizeAngle(-1e11)));
    EXPECT_TRUE(isRange(normalizeAngle(1e-11)));
    EXPECT_TRUE(isRange(normalizeAngle(-1e-11)));
}

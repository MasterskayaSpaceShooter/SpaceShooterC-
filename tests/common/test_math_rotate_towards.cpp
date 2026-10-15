#include <cmath>
#include <gtest/gtest.h>
#include <numbers>

#include "math_utils.h"

namespace {

using math::kEps;
using math::kPi;
using math::normalizeAngle;
using math::rotateTowards;
using math::Scalar;

constexpr Scalar kTol = 1e-9;
constexpr Scalar kTwoPi = 2 * kPi;

// углы сравниваются с учётом нормализации (для случаев через pi)
bool angleNear(Scalar a, Scalar b, Scalar tol = kTol) {
    return std::fabs(normalizeAngle(a - b)) < tol;
}

}  // namespace

// базовые сценарии

TEST(RotateTowards, BasicLimitedStep) {
    // current=0, target=pi/2, max_vel=1, dt=0.1 - шаг 0.1
    const Scalar result = rotateTowards(0.0, kPi / 2, 1.0, 0.1);
    EXPECT_NEAR(result, 0.1, kTol);
}

TEST(RotateTowards, ClampsToMaxStepForward) {
    // |delta| > max_step - двигаемся ровно на max_step
    const Scalar result = rotateTowards(0.0, 1.0, 1.0, 0.1);  // max_step=0.1
    EXPECT_NEAR(result, 0.1, kTol);
}

TEST(RotateTowards, ClampsToMaxStepBackward) {
    // короткий путь по часовой стрелке (отрицательное направление)
    const Scalar result = rotateTowards(0.0, -1.0, 1.0, 0.1);
    EXPECT_NEAR(result, -0.1, kTol);
}

// оба угла ненулевые, одинаковый знак
TEST(RotateTowards, BothPositiveLargeAngles) {
    // cur=2.0, tgt=2.5, delta=0.5, max_step=0.1 - 2.1
    EXPECT_NEAR(rotateTowards(2.0, 2.5, 1.0, 0.1), 2.1, kTol);
}

TEST(RotateTowards, BothNegativeLargeAngles) {
    EXPECT_NEAR(rotateTowards(-2.0, -2.5, 1.0, 0.1), -2.1, kTol);
}

// достижение цели / отсутствие перелёта

TEST(RotateTowards, ReachesTargetWhenClose) {
    const Scalar result = rotateTowards(0.0, 0.05, 1.0, 0.1);  // max_step=0.1 > 0.05
    EXPECT_NEAR(result, 0.05, kTol);
}

TEST(RotateTowards, AlreadyAtTarget) {
    const Scalar result = rotateTowards(1.0, 1.0, 1.0, 0.1);
    EXPECT_NEAR(result, 1.0, kTol);
}

TEST(RotateTowards, NoOvershootOnFinalTick) {
    // за один тик до цели |delta| < max_step, попадаем ровно в target
    const Scalar target = 1.5;
    const Scalar cur = 1.45;  // delta = 0.05 < 0.1
    const Scalar result = rotateTowards(cur, target, 1.0, 0.1);
    EXPECT_NEAR(result, target, kTol);
}

// границы условия |delta| <= max_step + kEps
TEST(RotateTowards, DeltaExactlyAtMaxStepReachesTarget) {
    // delta == max_step - ветка return tgt
    const Scalar result = rotateTowards(0.0, 0.1, 1.0, 0.1);
    EXPECT_NEAR(result, 0.1, kTol);
}

TEST(RotateTowards, DeltaSlightlyLessThanMaxStepReachesTarget) {
    const Scalar tgt = 0.1 - kEps / 2;
    const Scalar result = rotateTowards(0.0, tgt, 1.0, 0.1);
    EXPECT_NEAR(result, tgt, kTol);
}

TEST(RotateTowards, DeltaSlightlyGreaterThanMaxStepClamps) {
    // delta = max_step + 1e-6 > max_step - уходим в copysign
    const Scalar result = rotateTowards(0.0, 0.1 + 1e-6, 1.0, 0.1);
    EXPECT_NEAR(result, 0.1, kTol);  // cur + max_step
    EXPECT_GT(result, 0.1 - kTol);
    EXPECT_LT(result, 0.1 + 1e-6);
}

// при |delta| <= max_step возвращается именно tgt, а не cur + delta
// проверяем на ненулевом cur с коротким путём через 0.
TEST(RotateTowards, ReachesTargetPreservesExactValue) {
    const Scalar result = rotateTowards(3.0, -3.0, 100.0, 1.0);
    EXPECT_TRUE(angleNear(result, -3.0));
}

TEST(RotateTowards, ReachesTargetPreservesExactValueReverse) {
    // симметричный случай: cur=-3.0, tgt=+3.0
    const Scalar result = rotateTowards(-3.0, 3.0, 100.0, 1.0);
    EXPECT_TRUE(angleNear(result, 3.0));
}

TEST(RotateTowards, ZeroDtReturnsCurrentAngle) {
    // max_step = 0, до цели далеко - возвращаем текущий угол
    const Scalar result = rotateTowards(0.0, 1.0, 5.0, 0.0);
    EXPECT_NEAR(result, 0.0, kTol);
}

TEST(RotateTowards, ZeroAngularVelocityReturnsCurrentAngle) {
    const Scalar result = rotateTowards(0.0, 1.0, 0.0, 1.0);
    EXPECT_NEAR(result, 0.0, kTol);
}

TEST(RotateTowards, ZeroDtAtTargetReturnsTarget) {
    const Scalar result = rotateTowards(1.0, 1.0, 5.0, 0.0);
    EXPECT_NEAR(result, 1.0, kTol);
}

// кратчайший путь через pi

TEST(RotateTowards, ShortestPathAcrossPiForward) {
    const Scalar cur = 3.0;
    const Scalar tgt = -3.0;
    const Scalar result = rotateTowards(cur, tgt, 1.0, 0.1);
    EXPECT_TRUE(angleNear(result, cur + 0.1));
}

TEST(RotateTowards, ShortestPathAcrossPiBackward) {
    const Scalar cur = -3.0;
    const Scalar tgt = 3.0;
    const Scalar result = rotateTowards(cur, tgt, 1.0, 0.1);
    EXPECT_TRUE(angleNear(result, cur - 0.1));
}

// нормализация входов

TEST(RotateTowards, NormalizesLargeCurrentAngle) {
    const Scalar result = rotateTowards(0.0 + kTwoPi, kPi / 2, 1.0, 0.1);
    EXPECT_NEAR(result, 0.1, kTol);
}

TEST(RotateTowards, NormalizesLargeTargetAngle) {
    const Scalar result = rotateTowards(0.0, kPi / 2 + kTwoPi, 1.0, 0.1);
    EXPECT_NEAR(result, 0.1, kTol);
}

TEST(RotateTowards, NegativeLargeAngles) {
    const Scalar result = rotateTowards(-kTwoPi, 0.0, 1.0, 0.1);
    EXPECT_NEAR(result, 0.1, kTol);
}

TEST(RotateTowards, CurEqualsTargetPlusTwoPi) {
    const Scalar result = rotateTowards(kPi / 2 + kTwoPi, kPi / 2, 1.0, 0.1);
    EXPECT_NEAR(result, kPi / 2, kTol);
}

TEST(RotateTowards, HandlesTargetAtPiBoundary) {
    const Scalar result = rotateTowards(0.0, kPi, 1.0, 0.1);
    EXPECT_NEAR(result, 0.1, kTol);
}

// результат всегда в (-pi, pi]

TEST(RotateTowards, ResultAlwaysNormalized) {
    const Scalar cases[] = {0.0, 1.0, -1.0, 3.0, -3.0, kPi, -kPi, 10.0, -10.0};
    for (Scalar c : cases) {
        for (Scalar t : cases) {
            const Scalar r = rotateTowards(c, t, 0.5, 0.5);
            EXPECT_GT(r, -kPi);
            EXPECT_LE(r, kPi);
        }
    }
}

// асимметрия на границе pi (normalizeAngle(-pi) == +pi)

TEST(RotateTowards, BoundaryPiFoldsToPositivePi) {
    const Scalar r1 = rotateTowards(0.0, kPi, 100.0, 1.0);
    const Scalar r2 = rotateTowards(0.0, -kPi, 100.0, 1.0);
    EXPECT_NEAR(r1, kPi, kTol);
    EXPECT_NEAR(r2, kPi, kTol);  // не -pi!
}

// симметрия знаков валидна только для малых шагов, далеко от pi
TEST(RotateTowards, SignSymmetryForSmallSteps) {
    const Scalar r1 = rotateTowards(0.0, 1.0, 1.0, 0.1);
    const Scalar r2 = rotateTowards(0.0, -1.0, 1.0, 0.1);
    EXPECT_NEAR(r1, -r2, kTol);
}

// граничные значения max_step

TEST(RotateTowards, HugeVelocityReachesTargetInstantly) {
    const Scalar result = rotateTowards(0.0, 2.0, 1e9, 1.0);
    EXPECT_NEAR(result, 2.0, kTol);
}

TEST(RotateTowards, MaxStepEqualsFullTurnAlwaysReachesTarget) {
    EXPECT_NEAR(rotateTowards(0.0, 2.0, kTwoPi, 1.0), 2.0, kTol);
    EXPECT_NEAR(rotateTowards(3.0, -3.0, kTwoPi, 1.0), -3.0, kTol);
    EXPECT_NEAR(rotateTowards(-3.0, 3.0, kTwoPi, 1.0), 3.0, kTol);
}

// идемпотентность и сходимость

TEST(RotateTowards, IdempotentWhenAtTarget) {
    Scalar a = rotateTowards(0.0, 0.05, 1.0, 0.1);
    Scalar b = rotateTowards(a, 0.05, 1.0, 0.1);
    EXPECT_NEAR(b, 0.05, kTol);
}

TEST(RotateTowards, ConvergesOverMultipleTicks) {
    const Scalar target = kPi / 2;
    Scalar angle = 0.0;
    const Scalar max_vel = 1.0;
    const Scalar dt = 0.1;

    Scalar prev_dist = std::fabs(normalizeAngle(target - angle));
    for (int i = 0; i < 100; ++i) {
        angle = rotateTowards(angle, target, max_vel, dt);
        const Scalar dist = std::fabs(normalizeAngle(target - angle));
        // расстояние до цели не должно расти
        EXPECT_LE(dist, prev_dist + kTol);
        if (angleNear(angle, target))
            break;
        prev_dist = dist;
    }
    EXPECT_TRUE(angleNear(angle, target));
}

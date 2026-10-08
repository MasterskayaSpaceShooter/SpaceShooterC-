#include <gtest/gtest.h>
#include <math_utils.h>

namespace {

using math::Scalar;
using math::Vector2D;
using math::collision::resolveElasticImpulse;

void expectVecNear(const Vector2D& a, const Vector2D& e, Scalar tol = 1e-9) {
    EXPECT_NEAR(a.x, e.x, tol);
    EXPECT_NEAR(a.y, e.y, tol);
}

// Momentum and kinetic energy must both be conserved.
void expectConserved(const Vector2D& v1, Scalar m1, const Vector2D& v2, Scalar m2, const Vector2D& p0, Scalar e0) {
    expectVecNear(v1 * m1 + v2 * m2, p0);
    EXPECT_NEAR(0.5 * m1 * v1.lengthSquared() + 0.5 * m2 * v2.lengthSquared(), e0, 1e-9);
}

Scalar normalVel(const Vector2D& p1, const Vector2D& v1, const Vector2D& p2, const Vector2D& v2) {
    const Vector2D n = (p2 - p1).normalized();
    const Vector2D r = v2 - v1;
    return r.x * n.x + r.y * n.y;
}

// --- no-op cases ---

TEST(ResolveElasticImpulse, NoOpWhenNotApproaching) {
    const Vector2D p1(0, 0), p2(1, 0);

    // Separating
    {
        Vector2D v1(1, 0), v2(2, 0);
        resolveElasticImpulse(p1, v1, 1, p2, v2, 1);
        expectVecNear(v1, {1, 0});
        expectVecNear(v2, {2, 0});
    }

    // Tangent (vn == 0)
    {
        Vector2D v1(0, 1), v2(0, -1);
        resolveElasticImpulse(p1, v1, 1, p2, v2, 1);
        expectVecNear(v1, {0, 1});
        expectVecNear(v2, {0, -1});
    }

    // Both at rest
    {
        Vector2D v1(0, 0), v2(0, 0);
        resolveElasticImpulse(p1, v1, 1, p2, v2, 1);
        expectVecNear(v1, {0, 0});
        expectVecNear(v2, {0, 0});
    }
}

// --- head-on, equal masses ---

TEST(ResolveElasticImpulse, HeadOnEqualMassesSwap) {
    const Vector2D p1(-1, 0), p2(1, 0);
    Vector2D v1(1, 0), v2(-1, 0);

    resolveElasticImpulse(p1, v1, 1, p2, v2, 1);

    expectVecNear(v1, {-1, 0});
    expectVecNear(v2, {1, 0});
}

TEST(ResolveElasticImpulse, StationaryTargetEqualMass) {
    const Vector2D p1(-1, 0), p2(1, 0);
    Vector2D v1(1, 0), v2(0, 0);

    resolveElasticImpulse(p1, v1, 1, p2, v2, 1);

    expectVecNear(v1, {0, 0});
    expectVecNear(v2, {1, 0});
}

// --- conservation ---

TEST(ResolveElasticImpulse, HeadOnDifferentMassesConserves) {
    const Vector2D p1(-1, 0), p2(1, 0);
    Vector2D v1(1, 0), v2(-1, 0);
    const Scalar m1 = 2, m2 = 1;

    const Vector2D p0 = v1 * m1 + v2 * m2;
    const Scalar e0 = 0.5 * m1 * v1.lengthSquared() + 0.5 * m2 * v2.lengthSquared();

    resolveElasticImpulse(p1, v1, m1, p2, v2, m2);

    expectConserved(v1, m1, v2, m2, p0, e0);
    EXPECT_GE(normalVel(p1, v1, p2, v2), 0.0);
}

TEST(ResolveElasticImpulse, ObliqueCollisionConserves) {
    const Vector2D p1(0, 0), p2(1, 1);
    Vector2D v1(1, 0), v2(-1, 0);
    const Scalar m1 = 1.5, m2 = 2.5;

    const Vector2D p0 = v1 * m1 + v2 * m2;
    const Scalar e0 = 0.5 * m1 * v1.lengthSquared() + 0.5 * m2 * v2.lengthSquared();

    resolveElasticImpulse(p1, v1, m1, p2, v2, m2);

    expectConserved(v1, m1, v2, m2, p0, e0);
    EXPECT_GE(normalVel(p1, v1, p2, v2), 0.0);
}

}  // namespace

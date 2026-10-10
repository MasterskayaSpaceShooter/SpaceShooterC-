#pragma once

#include <cassert>
#include <cmath>
#include <numbers>

namespace math {

using Scalar = double;
constexpr Scalar kPi = std::numbers::pi_v<Scalar>;
constexpr double kEps = 1e-12;

struct Vector2D {
    Scalar x = 0;
    Scalar y = 0;

    Vector2D() = default;
    Vector2D(Scalar x, Scalar y);

    Vector2D& operator+=(const Vector2D& rhs);
    Vector2D& operator-=(const Vector2D& rhs);
    Vector2D& operator*=(Scalar s);

    [[nodiscard]] friend Vector2D operator+(Vector2D lhs, const Vector2D& rhs) {
        lhs += rhs;
        return lhs;
    }

    [[nodiscard]] friend Vector2D operator-(Vector2D lhs, const Vector2D& rhs) {
        lhs -= rhs;
        return lhs;
    }

    [[nodiscard]] friend Vector2D operator*(Vector2D lhs, Scalar s) {
        lhs *= s;
        return lhs;
    }

    [[nodiscard]] friend Vector2D operator*(Scalar s, Vector2D rhs) {
        rhs *= s;
        return rhs;
    }

    bool operator==(const Vector2D& other) const {
        return std::fabs(x - other.x) < kEps && std::fabs(y - other.y) < kEps;
    }

    [[nodiscard]] Scalar lengthSquared() const;
    [[nodiscard]] Scalar length() const;
    /// Единичный вектор того же направления.
    /// Для вектора нулевой длины возвращается нулевой вектор.
    [[nodiscard]] Vector2D normalized() const;
};

/// Wraps an angle in radians into (-pi, pi]; exactly -pi folds to +pi.
/// \param angle_rad angle in radians, any magnitude
/// \return equivalent angle in (-pi, pi]
[[nodiscard]] Scalar normalizeAngle(Scalar angle_rad);

/// Rotates `current_angle` toward `target_angle` by at most
/// `max_angular_velocity * dt` rad, taking the shortest path and
/// clamping to `target_angle` on the final tick (no overshoot).
/// \pre max_angular_velocity >= 0, dt >= 0
/// \return new heading in (-pi, pi]
[[nodiscard]] Scalar rotateTowards(Scalar current_angle, Scalar target_angle, Scalar max_angular_velocity, Scalar dt);

/// Advances ship `position` and `velocity` by one tick using
/// inertial integration with linear drag:
///   A = is_thrusting ? (cos angle, sin angle) * thrust_power : 0
///   V = (V + A*dt) * (1 - drag*dt);  P = P + V*dt
/// \pre thrust_power >= 0, drag in [0,1], dt >= 0, drag*dt <= 1
/// \pre `position` and `velocity` must not alias.
void updateShipPhysics(Vector2D& position,
                       Vector2D& velocity,
                       Scalar angle,
                       bool is_thrusting,
                       Scalar thrust_power,
                       Scalar drag,
                       Scalar dt);

/// Clamps `position` into the world rectangle centered at the origin:
/// x in [-world_width/2, +world_width/2], y in [-world_height/2, +world_height/2].
/// Per-axis clamp (inclusive bounds); corners are not treated radially.
/// \pre world_width > 0, world_height > 0
[[nodiscard]] Vector2D clampToWorldBounds(const Vector2D& position, Scalar world_width, Scalar world_height);

namespace collision {

/// True if two circles overlap or touch.
/// Criterion: (p1 - p2).lengthSquared() <= (r1 + r2)^2 —
/// inclusive of exact tangency.
/// \pre radius1 >= 0, radius2 >= 0
[[nodiscard]] bool checkCircleCollision(const Vector2D& pos1, Scalar radius1, const Vector2D& pos2, Scalar radius2);

/// Perfectly elastic linear impulse between two point masses at contact.
/// Positions are read-only (caller guarantees no overlap / uses CCD).
/// No-op if bodies are already separating.
/// \pre mass1 > 0, mass2 > 0
/// \pre pos1 != pos2
/// \post dot(vel2 - vel1, normalize(pos2 - pos1)) >= 0 (separating)
inline void resolveElasticImpulse(const Vector2D& pos1,
                                  Vector2D& vel1,
                                  Scalar mass1,
                                  const Vector2D& pos2,
                                  Vector2D& vel2,
                                  Scalar mass2) {
    assert(mass1 > 0 && mass2 > 0);
    // Unit normal from body 1 to body 2.
    const auto delta = pos2 - pos1;
    const auto sqr_dist = delta.x * delta.x + delta.y * delta.y;
    if (sqr_dist < kEps) {
        assert(false && "Coincident positions: contact normal undefined");
        return;
    }
    const auto n = delta * (1.0 / std::sqrt(sqr_dist));

    // Relative normal velocity. Negative means approaching.
    const auto rel_vel = vel2 - vel1;
    // TODO: change to .dot() after Vector2d implementation
    const auto vn = rel_vel.x * n.x + rel_vel.y * n.y;

    // Separating or tangent: no impulse needed.
    if (vn >= 0.0) {
        return;
    }

    // Inverse masses: lighter body receives larger velocity change.
    const auto inv_m1 = 1.0 / mass1;
    const auto inv_m2 = 1.0 / mass2;

    // Perfectly elastic impulse magnitude, e = 1.
    const auto j = -2.0 * vn / (inv_m1 + inv_m2);

    // Equal and opposite impulses along the contact normal.
    const auto impulse = n * j;
    vel1 -= impulse * inv_m1;
    vel2 += impulse * inv_m2;
}

}  // namespace collision

}  // namespace math

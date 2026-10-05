#pragma once

#include <boost/operators.hpp>
#include <numbers>

namespace math {

using Scalar = double;
constexpr Scalar kPi = std::numbers::pi_v<Scalar>;

struct Vector2D : boost::addable<Vector2D>,
                  boost::subtractable<Vector2D>,
                  boost::multipliable<Vector2D, Scalar>,
                  boost::multipliable2<Vector2D, Scalar>,
                  boost::equality_comparable<Vector2D> {
    Scalar x = 0;
    Scalar y = 0;

    Vector2D() = default;
    Vector2D(Scalar x, Scalar y);

    Vector2D& operator+=(const Vector2D& rhs);
    Vector2D& operator-=(const Vector2D& rhs);
    Vector2D& operator*=(Scalar s);

    bool operator==(const Vector2D&) const = default;

    [[nodiscard]] Scalar lengthSquared() const;
    [[nodiscard]] Scalar length() const;
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
void resolveElasticImpulse(const Vector2D& pos1,
                           Vector2D& vel1,
                           Scalar mass1,
                           const Vector2D& pos2,
                           Vector2D& vel2,
                           Scalar mass2);

}  // namespace collision

}  // namespace math

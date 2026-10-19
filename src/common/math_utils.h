#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>
#include <stdexcept>

#include "logger.h"

namespace math {

using Scalar = double;
constexpr Scalar kPi = std::numbers::pi_v<Scalar>;
constexpr double kEps = 1e-12;
constexpr Scalar kWorldHeight = 2000.0;
constexpr Scalar kWorldWidth = 2000.0;

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

    [[nodiscard]] Scalar lengthSquared() const noexcept;
    [[nodiscard]] Scalar length() const;
    [[nodiscard]] Scalar dot(const Vector2D& other) const noexcept;
    /// Единичный вектор того же направления.
    /// Для вектора нулевой длины возвращается нулевой вектор.
    [[nodiscard]] Vector2D normalized() const;
};

/// Wraps an angle in radians into (-pi, pi]; exactly -pi folds to +pi.
/// \param angle_rad angle in radians, any magnitude
/// \return equivalent angle in (-pi, pi]
[[nodiscard]] inline Scalar normalizeAngle(Scalar angle_rad) {
    constexpr Scalar two_kPi = 2 * kPi;
    Scalar ans = std::remainder(angle_rad, two_kPi);
    if (ans == -kPi)
        return kPi;
    return ans;
}

/// Rotates `current_angle` toward `target_angle` by at most
/// `max_angular_velocity * dt` rad, taking the shortest path and
/// clamping to `target_angle` on the final tick (no overshoot).
/// \pre max_angular_velocity >= 0, dt >= 0
/// \return new heading in (-pi, pi]
[[nodiscard]] inline Scalar rotateTowards(Scalar current_angle,
                                          Scalar target_angle,
                                          Scalar max_angular_velocity,
                                          Scalar dt) {
    const Scalar max_step = max_angular_velocity * dt;
    const Scalar cur = normalizeAngle(current_angle);
    const Scalar tgt = normalizeAngle(target_angle);
    const Scalar delta = normalizeAngle(tgt - cur);

    if (std::fabs(delta) <= max_step + kEps) {
        return tgt;
    }

    return normalizeAngle(cur + std::copysign(max_step, delta));
}

/// Advances ship `position` and `velocity` by one tick using
/// inertial integration with linear drag:
///   A = is_thrusting ? (cos angle, sin angle) * thrust_power : 0
///   V = (V + A*dt) * clamp(1 - drag*dt, 0, 1);  P = P + V*dt
///
/// If drag*dt > 1, drag fully stops the ship: `velocity` is zeroed
/// \pre thrust_power >= 0, drag in [0,1], dt >= 0
/// \pre `position` and `velocity` must not alias.
inline void updateShipPhysics(Vector2D& position,
                              Vector2D& velocity,
                              Scalar angle,
                              bool is_thrusting,
                              Scalar thrust_power,
                              Scalar drag,
                              Scalar dt,
                              Scalar max_speed = 600.0) {
    if (thrust_power < 0.0 || max_speed < 0.0 || drag < 0.0 || dt < 0.0) {
        LOG_ERROR("updateShipPhysics: недопустимые параметры: thrust_power={}, drag={}, dt={}", thrust_power, drag, dt);
    }

    if (is_thrusting) {
        velocity.x += std::cos(angle) * thrust_power * dt;
        velocity.y += std::sin(angle) * thrust_power * dt;
    }

    const Scalar drag_factor = std::clamp(1.0 - drag * dt, 0.0, 1.0);

    velocity *= drag_factor;

    const Scalar speed_sqr = velocity.lengthSquared();
    if (speed_sqr > max_speed * max_speed) {
        velocity *= max_speed / std::sqrt(speed_sqr);
    }

    constexpr Scalar kStopThresholdSqr = 0.01;  // 0.1 units/sec
    if (velocity.lengthSquared() < kStopThresholdSqr) {
        velocity = {};
    }

    position += velocity * dt;
}

/// Clamps `position` into the world rectangle centered at the origin:
/// x in [-world_width/2, +world_width/2], y in [-world_height/2, +world_height/2].
/// Per-axis clamp (inclusive bounds); corners are not treated radially.
/// Границы клампа отодвинуты на kEps внутрь от краёв мира, поэтому результат
/// всегда остаётся строго внутри мира. Прижимания к границе нет: координата
/// меняется только после реального выхода за дозволенный диапазон, без
/// «мёртвой зоны» вблизи края и обратного притягивания.
/// \pre world_width > 0, world_height > 0
[[nodiscard]] inline Vector2D clampToWorldBounds(const Vector2D& position,
                                                 Scalar world_width = kWorldWidth,
                                                 Scalar world_height = kWorldHeight) noexcept {
    const Scalar half_w = world_width * static_cast<Scalar>(0.5) - kEps;
    const Scalar half_h = world_height * static_cast<Scalar>(0.5) - kEps;

    return Vector2D{
        std::clamp(position.x, -half_w, half_w),
        std::clamp(position.y, -half_h, half_h),
    };
}

// Запрет неявных преобразований: аргументы world_width и world_height
// принимаются строго как double (Scalar); int/float и т.п. — ошибка компиляции.
template <typename T1, typename T2>
Vector2D clampToWorldBounds(const Vector2D&, T1, T2) = delete;

namespace collision {

/// True if two circles overlap or touch.
/// Criterion: (p1 - p2).lengthSquared() <= (r1 + r2)^2 —
/// inclusive of exact tangency.
/// \throws std::invalid_argument if either radius is not non-negative
[[nodiscard]] inline bool checkCircleCollision(const Vector2D& pos1,
                                               Scalar radius1,
                                               const Vector2D& pos2,
                                               Scalar radius2) {
    if (!(radius1 >= 0.0)) {
        throw std::invalid_argument("Radius1 must be non-negative.");
    }
    if (!(radius2 >= 0.0)) {
        throw std::invalid_argument("Radius2 must be non-negative.");
    }
    const auto delta = pos1 - pos2;
    const auto radius_sum = radius1 + radius2;
    return delta.lengthSquared() <= radius_sum * radius_sum;
}

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
    const auto vn = rel_vel.dot(n);

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

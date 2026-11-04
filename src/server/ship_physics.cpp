#include "ship_physics.h"

namespace domain::ship_physics {

constexpr math::Scalar kMass = 1.0;
constexpr math::Scalar kMaxAccel = 500.0;
constexpr math::Scalar kMaxSpeed = 300.0;
constexpr math::Scalar kLinearDrag = 1.5;
constexpr math::Scalar kStopThreshold = 0.1;

[[nodiscard]] math::Vector2D clampToUnitLength(const math::Vector2D& v) noexcept {
    const auto len_sq = v.lengthSquared();
    if (len_sq <= 1.0)
        return v;
    return v * (1.0 / std::sqrt(len_sq));
}

void stepShip(TransformComponent& t, math::Vector2D move_direction, math::Scalar dt) {
    if (dt <= 0.0)
        return;

    const math::Vector2D dir = clampToUnitLength(move_direction);

    t.velocity += dir * (kMaxAccel / kMass * dt);

    const math::Scalar speed_sq = t.velocity.lengthSquared();
    if (speed_sq > kMaxSpeed * kMaxSpeed) {
        t.velocity *= kMaxSpeed / std::sqrt(speed_sq);
    }

    t.velocity *= std::max(0.0, 1.0 - kLinearDrag * dt);

    const bool moving = t.velocity.lengthSquared() >= kStopThreshold * kStopThreshold;
    if (!moving) {
        t.velocity = {};
    }

    t.position += t.velocity * dt;
    if (moving) {
        t.angle = std::atan2(t.velocity.y, t.velocity.x);
    }
}
}  // namespace domain::ship_physics

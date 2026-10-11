#include "math_utils.h"

namespace math { namespace collision {

[[nodiscard]] bool checkCircleCollision(const Vector2D& pos1, Scalar radius1, const Vector2D& pos2, Scalar radius2) {
    const auto delta = pos1 - pos2;
    const auto radius_sum = radius1 + radius2;
    return delta.lengthSquared() <= radius_sum * radius_sum;
}

void resolveElasticImpulse(const Vector2D& pos1,
                           Vector2D& vel1,
                           Scalar mass1,
                           const Vector2D& pos2,
                           Vector2D& vel2,
                           Scalar mass2) {
    const auto n = (pos2 - pos1).normalized();
    const auto rel_vel = vel2 - vel1;
    const auto vn = rel_vel.x * n.x + rel_vel.y * n.y;

    if (vn >= 0.0) {
        return;
    }

    const auto inv_m1 = 1.0 / mass1;
    const auto inv_m2 = 1.0 / mass2;
    const auto j = -2.0 * vn / (inv_m1 + inv_m2);

    vel1 -= n * (j * inv_m1);
    vel2 += n * (j * inv_m2);
}

}}  // namespace math::collision

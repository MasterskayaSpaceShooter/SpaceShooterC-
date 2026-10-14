#pragma once

#include <cmath>

#include "entity.h"

namespace domain {

/**
 * @brief Integrate motion for all live entities and resolve world bounds.
 *
 * @tparam EntityContainer Any range whose value_type is Entity
 *         (std::vector<Entity>, std::deque<Entity>, ...).
 *
 * @param entities     Container of entities. Destroyed entities and entities
 *                     without a TransformComponent are skipped.
 * @param dt           Frame duration.
 *
 * @pre dt >= 0, world_width > 0, world_height > 0.
 *
 * @post Every non-destroyed entity with a transform has its position clamped
 *       into the world rectangle.
 * @post Bullets and asteroids whose integration step left the world are marked
 *       isDestroyed = true; ships are clamped instead.
 * @post Ships that hit a wall keep their tangential velocity (sliding); only
 *       the velocity component normal to the wall is zeroed.
 *
 * @note Uses discrete integration; fast bullets can tunnel through small
 *       targets.
 * @note Mutates entity.isDestroyed but does not reclaim storage; call
 *       EntityPool::recycleDestroyed() once per frame afterwards.
 */
template <typename EntityContainer>
inline void updateMovementAndBounds(EntityContainer& entities,
                                    math::Scalar dt,
                                    math::Scalar world_width,
                                    math::Scalar world_height) {
    for (auto& entity : entities) {
        if (entity.type == EntityType::None || entity.isDestroyed || !entity.transform.has_value()) {
            continue;
        }

        auto& t = *entity.transform;

        if (entity.type == EntityType::Ship && entity.ship.has_value()) {
            auto& s = *entity.ship;
            math::updateShipPhysics(t.position, t.velocity, t.angle, s.is_thrusting, s.thrust_power, s.drag, dt);
        } else {
            t.position += t.velocity * dt;
        }

        const math::Vector2D clamped = math::clampToWorldBounds(t.position, world_width, world_height);

        if (clamped == t.position) {
            continue;
        }

        switch (entity.type) {
            case EntityType::None:
                // unreachable, but safety-net
                break;

            case EntityType::Bullet:
                entity.isDestroyed = true;
                break;

            case EntityType::Ship:
                if (std::abs(clamped.x - t.position.x) > math::kEps) {
                    t.velocity.x = 0.0;
                }
                if (std::abs(clamped.y - t.position.y) > math::kEps) {
                    t.velocity.y = 0.0;
                }
                t.position = clamped;
                break;

            case EntityType::Asteroid:
                entity.isDestroyed = true;
                break;
        }
    }
}

}  // namespace domain

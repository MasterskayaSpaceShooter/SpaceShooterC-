#pragma once

#include <cmath>
#include <optional>
#include <type_traits>

#include "entity.h"

namespace domain {

/**
 * @brief Integrate motion for all live entities and resolve world bounds.
 *
 * @tparam EntityContainer Range of Entity or std::optional<Entity>.
 *         Works with EntityPool::storage() directly.
 *
 * @param entities     Container of entities. Empty optionals, destroyed
 *                     entities, and entities without a transform are skipped.
 * @param dt           Frame duration.
 *
 * @pre dt >= 0, world_width > 0, world_height > 0.
 *
 * @post Bullets that leave the world are marked isDestroyed = true.
 * @post Ships are clamped to the world rectangle; wall hits zero only the
 *       velocity component normal to the wall (sliding).
 * @post Asteroids wrap around the world edges once their full radius has
 *       left the world rectangle.
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
    using ValueType = typename EntityContainer::value_type;

    for (auto& slot : entities) {
        // Unwrap std::optional<Entity> slots (EntityPool::storage()).
        Entity* entity_ptr = nullptr;
        if constexpr (std::is_same_v<ValueType, std::optional<Entity>>) {
            if (!slot.has_value()) {
                continue;
            }
            entity_ptr = &*slot;
        } else {
            entity_ptr = &slot;
        }

        Entity& entity = *entity_ptr;

        if (entity.type == EntityType::None || entity.isDestroyed || !entity.transform.has_value()) {
            continue;
        }

        auto& t = *entity.transform;

        // Integrate.
        if (entity.type == EntityType::Ship && entity.ship.has_value()) {
            auto& s = *entity.ship;
            math::updateShipPhysics(t.position, t.velocity, t.angle, s.is_thrusting, s.thrust_power, s.drag, dt);
        } else {
            t.position += t.velocity * dt;
        }

        // Asteroids wrap once fully outside the world.
        if (entity.type == EntityType::Asteroid) {
            const math::Scalar r = entity.radius;

            if (t.position.x + r < 0.0) {
                t.position.x = world_width + r;
            } else if (t.position.x - r > world_width) {
                t.position.x = -r;
            }

            if (t.position.y + r < 0.0) {
                t.position.y = world_height + r;
            } else if (t.position.y - r > world_height) {
                t.position.y = -r;
            }

            continue;
        }

        // Clamp for everything else.
        const math::Vector2D clamped = math::clampToWorldBounds(t.position, world_width, world_height);

        // Epsilon-safe comparison: did clamping actually move us on each axis?
        const bool clamped_x = std::abs(clamped.x - t.position.x) > math::kEps;
        const bool clamped_y = std::abs(clamped.y - t.position.y) > math::kEps;

        if (!clamped_x && !clamped_y) {
            continue;
        }

        switch (entity.type) {
            case EntityType::None:
                // Safety net; unreachable.
                break;

            case EntityType::Bullet:
                entity.isDestroyed = true;
                break;

            case EntityType::Ship:
                if (clamped_x) {
                    t.velocity.x = 0.0;
                }
                if (clamped_y) {
                    t.velocity.y = 0.0;
                }
                t.position = clamped;
                break;

            case EntityType::Asteroid:
                // Unreachable: asteroids are handled (wrapped) above.
                break;
        }
    }
}

}  // namespace domain

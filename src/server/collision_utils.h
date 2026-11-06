#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include "entity.h"
#include "entity_utils.h"
#include "health_utils.h"
#include "logger.h"
#include "math_utils.h"

namespace domain::collision {

inline void resolveElasticImpulse(const math::Vector2D& pos1,
                                  math::Vector2D& vel1,
                                  math::Scalar mass1,
                                  const math::Vector2D& pos2,
                                  math::Vector2D& vel2,
                                  math::Scalar mass2,
                                  math::Scalar restitution = 1.0) noexcept {
    if (!std::isfinite(mass1) || !std::isfinite(mass2) || mass1 <= 0.0 || mass2 <= 0.0) {
        return;
    }

    const auto delta = pos2 - pos1;
    const auto dist_sq = delta.lengthSquared();
    if (dist_sq < math::kEps * math::kEps) {
        return;
    }

    const auto dist = std::sqrt(dist_sq);
    const auto inv_dist = 1.0 / dist;
    const auto normal = delta * inv_dist;

    const auto rel_vel = vel2 - vel1;
    const auto vel_along_normal = rel_vel.dot(normal);

    if (vel_along_normal >= 0.0) {
        return;
    }

    const auto clamped_restitution = std::clamp(restitution, 0.0, 1.0);
    const auto impulse_magnitude =
        -(1.0 + clamped_restitution) * vel_along_normal / (1.0 / mass1 + 1.0 / mass2);

    const auto impulse = normal * impulse_magnitude;
    vel1 -= impulse * (1.0 / mass1);
    vel2 += impulse * (1.0 / mass2);
}

inline void applyPercentageDamage(Entity& entity, std::uint16_t percentage) noexcept {
    if (!entity.health) {
        return;
    }

    const auto damage = static_cast<std::uint16_t>(
        (static_cast<std::uint32_t>(entity.health->max) * percentage) / 100);
    applyDamage(*entity.health, damage);
    if (!isAlive(*entity.health)) {
        entity.isDestroyed = true;
    }
}

inline void repelAsteroid(Entity& ship, Entity& asteroid) noexcept {
    if (!ship.transform || !asteroid.transform || !std::isfinite(ship.mass) ||
        !std::isfinite(asteroid.mass) || ship.mass <= 0.0 || asteroid.mass <= 0.0) {
        return;
    }

    auto asteroid_position = asteroid.transform->position;
    const auto separation = asteroid_position - ship.transform->position;
    if (separation.lengthSquared() < math::kEps) {
        const auto angle = std::isfinite(ship.transform->angle) ? ship.transform->angle : 0.0;
        asteroid_position = ship.transform->position + math::Vector2D(std::cos(angle), std::sin(angle));
    }

    math::collision::resolveElasticImpulse(ship.transform->position,
                                           ship.transform->velocity,
                                           ship.mass,
                                           asteroid_position,
                                           asteroid.transform->velocity,
                                           asteroid.mass);
}

inline void onCollisionShipShip(Entity& ship1, Entity& ship2) noexcept {
    if (!ship1.transform || !ship2.transform) {
        return;
    }

    resolveElasticImpulse(ship1.transform->position,
                          ship1.transform->velocity,
                          ship1.mass,
                          ship2.transform->position,
                          ship2.transform->velocity,
                          ship2.mass);
}

template <typename CanGrowPredicate>
inline void onCollisionShipAsteroid(Entity& ship, Entity& asteroid, CanGrowPredicate&& can_grow) noexcept {
    if (!ship.transform || !asteroid.transform) {
        return;
    }

    if (ship.type == types::EntityType::Asteroid && asteroid.type == types::EntityType::Ship) {
        onCollisionShipAsteroid(asteroid, ship, std::forward<CanGrowPredicate>(can_grow));
        return;
    }

    if (ship.type != types::EntityType::Ship || asteroid.type != types::EntityType::Asteroid) {
        return;
    }

    if (!ship.isValid() || !asteroid.isValid()) {
        return;
    }

    if (ship.mass >= asteroid.mass) {
        const auto new_mass = ship.mass + asteroid.mass;
        const auto new_radius = std::sqrt(ship.radius * ship.radius + asteroid.radius * asteroid.radius);

        if (std::forward<CanGrowPredicate>(can_grow)(ship.transform->position, new_radius)) {
            ship.mass = new_mass;
            ship.radius = new_radius;
        }

        if (ship.ship && asteroid.asteroid) {
            ship.ship->score += asteroid.asteroid->score_reward;
        }

        asteroid.isDestroyed = true;
    } else {
        std::uint16_t damage_percent = 10;
        if (asteroid.radius >= 40.0) {
            damage_percent = 50;
        } else if (asteroid.radius >= 20.0) {
            damage_percent = 25;
        }

        applyPercentageDamage(ship, damage_percent);
        repelAsteroid(ship, asteroid);
    }
}

inline void onCollisionShipAsteroid(Entity& ship, Entity& asteroid) noexcept {
    onCollisionShipAsteroid(ship, asteroid, [](auto&, auto) noexcept { return true; });
}

template <typename OnHit>
inline void onCollisionBulletAsteroid(Entity& bullet, Entity& asteroid, OnHit&& on_hit) {
    if (bullet.type == types::EntityType::Asteroid && asteroid.type == types::EntityType::Bullet) {
        onCollisionBulletAsteroid(asteroid, bullet, std::forward<OnHit>(on_hit));
        return;
    }

    if (bullet.type != types::EntityType::Bullet || asteroid.type != types::EntityType::Asteroid) {
        return;
    }

    if (!bullet.isValid() || !asteroid.isValid() || bullet.isDestroyed || asteroid.isDestroyed) {
        return;
    }

    // Наносим урон астероиду
    const std::uint16_t damage = bullet.combat ? bullet.combat->damage : 25;
    if (asteroid.health) {
        domain::applyDamage(*asteroid.health, damage);
        if (!domain::isAlive(*asteroid.health)) {
            asteroid.isDestroyed = true;
        }
    } else {
        asteroid.isDestroyed = true;
    }

    bullet.isDestroyed = true;

    types::EntityId owner_id = bullet.combat ? bullet.combat->owner_id : types::EntityId(0);
    const bool asteroid_destroyed = asteroid.isDestroyed;

    std::forward<OnHit>(on_hit)(owner_id, asteroid_destroyed);
}

inline void onCollisionBulletAsteroid(Entity& bullet, Entity& asteroid) noexcept {
    onCollisionBulletAsteroid(bullet, asteroid, [](types::EntityId, bool) {});
}


template <typename OnHit>
inline void onCollisionBulletShip(Entity& bullet, Entity& ship, OnHit&& on_hit) {
    if (bullet.type == types::EntityType::Ship && ship.type == types::EntityType::Bullet) {
        onCollisionBulletShip(ship, bullet, std::forward<OnHit>(on_hit));
        return;
    }

    if (bullet.type != types::EntityType::Bullet || ship.type != types::EntityType::Ship) {
        return;
    }

    if (!bullet.isValid() || !ship.isValid()) {
        return;
    }

    if (!bullet.combat || !ship.health || !ship.ship || !ship.isDamageable()) {
        return;
    }

    const types::EntityId owner_id = bullet.combat->owner_id;

    if (owner_id == ship.id) {
        return;
    }

    const std::uint16_t damage = bullet.combat->damage;
    domain::applyDamage(*ship.health, damage);

    bullet.isDestroyed = true;

    const bool ship_destroyed = !domain::isAlive(*ship.health);
    if (ship_destroyed) {
        ship.isDestroyed = true;
        LOG_INFO("[Collision] Bullet destroyed ship {} (owner: {})", ship.id.get(), owner_id.get());
    }

    std::forward<OnHit>(on_hit)(owner_id, ship_destroyed);
}

inline void onCollisionBulletShip(Entity& bullet, Entity& ship) noexcept {
    onCollisionBulletShip(bullet, ship, [](types::EntityId, bool) {});
}

[[nodiscard]] inline bool circleIntersectsOppositeWorldBounds(const math::Vector2D& pos,
                                                             math::Scalar radius,
                                                             math::Scalar world_width,
                                                             math::Scalar world_height) noexcept {
    if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(radius) ||
        !std::isfinite(world_width) || !std::isfinite(world_height) || radius < 0.0 ||
        world_width <= 0.0 || world_height <= 0.0) {
        return false;
    }

    const auto half_w = world_width * 0.5;
    const auto half_h = world_height * 0.5;

    const bool intersects_horizontal = (pos.x - radius < -half_w) && (pos.x + radius > half_w);
    const bool intersects_vertical = (pos.y - radius < -half_h) && (pos.y + radius > half_h);

    return intersects_horizontal || intersects_vertical;
}

[[nodiscard]] inline bool isShipCollisionValid(const Entity& entity) noexcept {
    return entity.type == types::EntityType::Ship && entity.isValid() && entity.transform.has_value() &&
           entity.ship.has_value();
}

[[nodiscard]] inline bool isAsteroidCollisionValid(const Entity& entity) noexcept {
    return entity.type == types::EntityType::Asteroid && entity.isValid() && entity.transform.has_value() &&
           entity.asteroid.has_value();
}

[[nodiscard]] inline bool isBulletCollisionValid(const Entity& entity) noexcept {
    return entity.type == types::EntityType::Bullet && entity.isValid() && entity.transform.has_value() &&
           entity.combat.has_value();
}

}  // namespace domain::collision

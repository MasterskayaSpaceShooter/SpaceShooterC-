#pragma once

#include <cmath>
#include <cstdint>

#include "entity.h"

namespace domain::collision {

namespace detail {

inline void applyPercentageDamage(Entity&, std::uint16_t) noexcept {}

inline void repelAsteroid(Entity&, Entity&) {}

}  // namespace detail

/**
 * @brief Resolves a ship-asteroid collision based on their mass ratio.
 *
 * The ship must be the first argument. Entities without the required
 * components, invalid masses, and destroyed entities are ignored.
 */
inline void onCollision(Entity& ship, Entity& asteroid) {
    if (ship.type != types::EntityType::Ship || asteroid.type != types::EntityType::Asteroid || ship.isDestroyed ||
        asteroid.isDestroyed || !ship.transform || !asteroid.transform || !ship.health || !std::isfinite(ship.mass) ||
        !std::isfinite(asteroid.mass) || !(ship.mass > 0.0) || !(asteroid.mass > 0.0)) {
        return;
    }

    if (ship.mass >= 5.0 * asteroid.mass) {
        detail::repelAsteroid(ship, asteroid);
        return;
    }

    if (asteroid.mass < 0.5 * ship.mass) {
        ship.mass += asteroid.mass;
        asteroid.isDestroyed = true;
        return;
    }

    if (asteroid.mass <= ship.mass) {
        detail::applyPercentageDamage(ship, 25);
        detail::applyPercentageDamage(asteroid, 25);
        return;
    }

    detail::applyPercentageDamage(ship, 50);
}

}  // namespace domain::collision

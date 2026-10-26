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
    if (ship.type == types::EntityType::Asteroid && asteroid.type == types::EntityType::Ship) {
        onCollision(asteroid, ship);
        return;
    }

    if (ship.type != types::EntityType::Ship || asteroid.type != types::EntityType::Asteroid) {
        return;
    }

    if (!ship.isValid() || !asteroid.isValid()) {
        return;
    }

    if (!ship.isDamageable() || !asteroid.isDamageable()) {
        return;
    }

    const math::Scalar mass_ratio = asteroid.mass / ship.mass;

    // 1. Слишком мелкий астероид (<= 20% массы корабля):
    // Большой корабль отталкивает его, оставляя добычу мелким игрокам
    if (mass_ratio <= 0.2) {
        detail::repelAsteroid(ship, asteroid);
        return;
    }

    // 2. Съедобный диапазон (от 20% до 50% массы корабля):
    // Корабль впитывает массу астероида
    if (mass_ratio < 0.5) {
        ship.addMass(asteroid.mass);
        asteroid.isDestroyed = true;
        return;
    }

    // 3. Сопоставимые массы (от 50% до 100%):
    // Серьёзное столкновение — оба получают по 25% урона
    if (mass_ratio <= 1.0) {
        detail::applyPercentageDamage(ship, 25);
        detail::applyPercentageDamage(asteroid, 25);
        return;
    }

    // 4. Астероид тяжелее корабля (> 100%):
    // Удар о скалу — 50% урона кораблю
    detail::applyPercentageDamage(ship, 50);
}

}  // namespace domain::collision

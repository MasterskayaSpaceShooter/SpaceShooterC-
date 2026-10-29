#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

#include "entity.h"
#include "health_utils.h"
#include "logger.h"
#include "score_utils.h"

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

    if (ship.mass <= 0 || asteroid.mass <= 0) {
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
        if (ship.addMass(asteroid.mass)) {
            asteroid.isDestroyed = true;
        }
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

/**
 * @brief Resolves a collision between two ships.
 *
 * The lighter ship takes 10 damage, the heavier ship takes 5.
 * Ships with equal mass take 7 damage each.
 * Both ships lose 5 score points. If one ship is destroyed,
 * the surviving ship receives 30 points and one frag.
 */
inline void onCollisionShipShip(Entity& ship1, Entity& ship2) {
    if (ship1.type != types::EntityType::Ship || ship2.type != types::EntityType::Ship) {
        LOG_WARN("[Collision] Ship-ship collision ignored: entities {} and {} are not ships",
                 ship1.id.get(),
                 ship2.id.get());
        return;
    }

    if (!ship1.isValid() || !ship2.isValid()) {
        LOG_WARN("[Collision] Ship-ship collision ignored: invalid entities {} and {}", ship1.id.get(), ship2.id.get());
        return;
    }

    if (!ship1.isDamageable() || !ship2.isDamageable()) {
        LOG_WARN("[Collision] Ship-ship collision ignored: entities {} and {} are not damageable",
                 ship1.id.get(),
                 ship2.id.get());
        return;
    }

    if (!ship1.ship || !ship2.ship) {
        LOG_WARN("[Collision] Ship-ship collision ignored: missing ship component for entities {} and {}",
                 ship1.id.get(),
                 ship2.id.get());
        return;
    }

    // Определяем урон в зависимости от массы кораблей.
    std::uint16_t damage1 = 7;
    std::uint16_t damage2 = 7;

    if (ship1.mass < ship2.mass) {
        damage1 = 10;
        damage2 = 5;
    } else if (ship1.mass > ship2.mass) {
        damage1 = 5;
        damage2 = 10;
    }

    // Штраф за столкновение получают оба корабля.
    domain::subtractScore(*ship1.ship, 5);
    domain::subtractScore(*ship2.ship, 5);

    // Наносим урон и отмечаем уничтоженные корабли.
    domain::applyDamage(*ship1.health, damage1);
    domain::applyDamage(*ship2.health, damage2);

    if (!domain::isAlive(*ship1.health)) {
        ship1.isDestroyed = true;
    }

    if (!domain::isAlive(*ship2.health)) {
        ship2.isDestroyed = true;
    }

    // Награда начисляется только выжившему кораблю,
    // если в результате столкновения уничтожен соперник.
    if (ship1.isDestroyed && !ship2.isDestroyed) {
        domain::addScore(*ship2.ship, 30);

        if (ship2.ship->frags < std::numeric_limits<std::uint16_t>::max()) {
            ++ship2.ship->frags;
        }
    } else if (ship2.isDestroyed && !ship1.isDestroyed) {
        domain::addScore(*ship1.ship, 30);

        if (ship1.ship->frags < std::numeric_limits<std::uint16_t>::max()) {
            ++ship1.ship->frags;
        }
    }
}

}  // namespace domain::collision

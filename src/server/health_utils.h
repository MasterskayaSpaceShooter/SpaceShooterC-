
#pragma once

#include <cstdint>

#include "entity.h"

namespace domain {

/**
 * @brief Applies damage to a health component.
 * @param health Health component to modify.
 * @param damage Amount of damage.
 * @post health.current is not less than zero.
 */
inline void applyDamage(HealthComponent& health, std::uint16_t damage) {
    if (damage >= health.current) {
        health.current = 0;
        return;
    }

    health.current -= damage;
}

/**
 * @brief Restores health without exceeding its maximum value.
 * @param health Health component to modify.
 * @param amount Amount of health to restore.
 * @post health.current does not exceed health.max.
 */
inline void restoreHealth(HealthComponent& health, std::uint16_t amount) {
    if (health.current >= health.max) {
        health.current = health.max;
        return;
    }

    const std::uint16_t missing = health.max - health.current;

    if (amount >= missing) {
        health.current = health.max;
        return;
    }

    health.current += amount;
}

/**
 * @brief Checks whether the entity has positive health.
 */
[[nodiscard]] inline bool isAlive(const HealthComponent& health) {
    return health.current > 0;
}

}  // namespace domain

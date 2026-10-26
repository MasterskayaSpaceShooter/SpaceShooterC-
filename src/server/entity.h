#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>

#include "math_utils.h"
#include "types.h"

namespace domain {

// TODO: move default values to config
// Components
struct TransformComponent {
    math::Vector2D position;
    math::Vector2D velocity;
    math::Scalar angle{0.0};
};

struct ShipComponent {
    std::array<char, 32> name{};
    std::uint32_t score{0};
    math::Scalar thrust_power{0.0};
    math::Scalar drag{0.0};
    bool is_thrusting{false};
    std::uint16_t frags{0};
};

struct HealthComponent {
    std::uint16_t current{100};
    std::uint16_t max{100};
};

struct CombatComponent {
    std::uint16_t damage{0};
    types::EntityId owner_id{0};
};

struct AsteroidComponent {
    std::uint16_t score_reward{0};
};

/**
 * @brief Hybrid Entity container for the MVP (struct-of-optionals, not a true ECS).
 *
 * @note Technical trade-offs:
 *  - Memory heavy: storage sized for the union of all components, even when nullopt.
 *
 * @note Refactor path: contiguous component vectors (e.g. EnTT) once profiling shows this is hot.
 */
struct Entity {
    static constexpr math::Scalar kInitialShipRadius = 5.0;

    types::EntityId id{0};
    types::EntityType type{types::EntityType::None};
    math::Scalar mass{0.0};
    math::Scalar radius{0.0};
    bool isDestroyed{false};

    std::optional<TransformComponent> transform;
    std::optional<ShipComponent> ship;
    std::optional<HealthComponent> health;
    std::optional<CombatComponent> combat;
    std::optional<AsteroidComponent> asteroid;

    [[nodiscard]] static constexpr math::Scalar massForRadius(math::Scalar radius) noexcept {
        return std::numbers::pi_v<math::Scalar> * radius * radius / 2.0;
    }

    void addMass(math::Scalar amount) {
        if (type != types::EntityType::Ship) {
            throw std::logic_error("Only ships can accumulate mass");
        }
        if (!std::isfinite(amount) || amount < 0.0) {
            throw std::invalid_argument("Mass to add must be finite and non-negative");
        }
        if (!std::isfinite(mass) || mass < 0.0 || !std::isfinite(radius) || radius <= 0.0) {
            throw std::logic_error("Ship must have a valid mass and radius");
        }

        const math::Scalar new_mass = mass + amount;
        if (!std::isfinite(new_mass)) {
            throw std::overflow_error("Ship mass overflow");
        }

        mass = new_mass;
        while (radius <= std::numeric_limits<math::Scalar>::max() / 2.0) {
            const math::Scalar next_radius = radius * 2.0;
            const math::Scalar target_mass = massForRadius(next_radius);
            if (!std::isfinite(target_mass) || mass < target_mass) {
                break;
            }
            radius = next_radius;
        }
    }

    [[nodiscard]] bool isValid() const noexcept {
        return type != types::EntityType::None && !isDestroyed && transform.has_value() && std::isfinite(mass) &&
               mass >= 0.0 && std::isfinite(radius) && radius >= 0.0;
    }

    [[nodiscard]] bool isDamageable() const noexcept {
        return health.has_value() && health->current > 0;
    }
};

}  // namespace domain

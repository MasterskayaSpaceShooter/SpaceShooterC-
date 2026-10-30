#pragma once

#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>

#include "math_utils.h"
#include "types.h"

namespace domain {

template <typename Predicate>
concept NothrowGrowthPredicate = std::predicate<Predicate&, math::Vector2D&, math::Scalar> &&
                                 requires(Predicate& predicate, math::Vector2D& position, math::Scalar radius) {
                                     { predicate(position, radius) } noexcept;
                                 };

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

    [[nodiscard]] bool addMass(math::Scalar amount) noexcept {
        return addMass(amount, [](math::Vector2D&, math::Scalar) noexcept {
            return true;
        });
    }

    [[nodiscard]] bool addMass(math::Scalar amount, NothrowGrowthPredicate auto can_grow) noexcept {
        if (type != types::EntityType::Ship) {
            LOG_ERROR("Only ships can accumulate mass");
            return false;
        }
        if (!std::isfinite(amount) || amount < 0.0) {
            LOG_ERROR("Mass to add must be finite and non-negative");
            return false;
        }
        if (!std::isfinite(mass) || mass < 0.0 || !std::isfinite(radius) || radius <= 0.0 || !transform) {
            LOG_ERROR("Ship must have a valid mass and radius");
            return false;
        }

        const math::Scalar new_mass = mass + amount;
        if (!std::isfinite(new_mass)) {
            LOG_ERROR("Ship mass overflow");
            return false;
        }

        mass = new_mass;
        while (radius <= std::numeric_limits<math::Scalar>::max() / 2.0) {
            const math::Scalar next_radius = radius * 2.0;
            const math::Scalar target_mass = massForRadius(next_radius);
            if (!std::isfinite(target_mass) || mass < target_mass) {
                break;
            }

            if (!transform.has_value()) {
                LOG_ERROR("Ship has no transform");
                return false;
            }

            if (!can_grow(transform->position, next_radius)) {
                break;
            }

            radius = next_radius;
        }
        return true;
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

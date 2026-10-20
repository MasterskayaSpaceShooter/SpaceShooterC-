#pragma once

#include <array>
#include <cstdint>
#include <optional>

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
};

}  // namespace domain

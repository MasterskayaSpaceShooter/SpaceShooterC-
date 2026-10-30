#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

#include "math_utils.h"
#include "types.h"

namespace snapshot {

struct TransformSnapshot {
    math::Vector2D position;
    math::Vector2D velocity;
    math::Scalar angle;
};
using EntityId = std::uint64_t;

struct ShipSnapshot {
    std::string name;
    std::uint32_t score;
    math::Scalar thrust_power;
    math::Scalar drag;
    bool is_thrusting;
    std::uint16_t frags;
};

struct HealthSnapshot {
    std::uint16_t current;
    std::uint16_t max;
};

struct CombatSnapshot {
    std::uint16_t damage;
    types::EntityId owner_id;
};

struct AsteroidSnapshot {
    std::uint16_t score_reward{0};
};

/// The EntitySnapshot mirrors domain::Entity 1:1
struct EntitySnapshot {
    types::EntityId id;
    types::EntityType type;
    math::Scalar mass;
    math::Scalar radius;
    bool is_destroyed;

    std::optional<TransformSnapshot> transform;
    std::optional<ShipSnapshot> ship;
    std::optional<HealthSnapshot> health;
    std::optional<CombatSnapshot> combat;
    std::optional<AsteroidSnapshot> asteroid;
};

}  // namespace snapshot

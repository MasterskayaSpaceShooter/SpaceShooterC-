#pragma once

#include <cstdint>
#include <string>

#include "math_utils.h"

namespace snapshot {

using EntityId = int;

enum class EntityType { Ship, Projectile, Asteroid };

class EntitySnapshot {
    int id_;
    EntityType type_;
    math::Vector2D position_;
    math::Vector2D velocity_;
    float radius_;
    float rotation_angle_;
    std::uint32_t score_;
    std::string name_;
};

}  // namespace snapshot

#pragma once

#include <cstdint>

namespace types {

using EntityId = std::uint64_t;

enum class EntityType : std::uint8_t { None, Ship, Asteroid, Bullet };

}  // namespace types

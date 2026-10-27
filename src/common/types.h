#pragma once

#include <cstdint>

#include "strong_id.h"

namespace types {

/// Tag to differentiate StrongId types
struct EntityIdTag {};

using EntityId = types::StrongId<EntityIdTag>;

using EntityHash = types::StrongIdIdentityHash<EntityId>;

enum class EntityType : std::uint8_t { None, Ship, Asteroid, Bullet };

enum class Direction { Up, Down, Left, Right };

}  // namespace types

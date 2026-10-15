#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "entity_snapshot.h"

namespace snapshot_wire {

/// Serialize one entity into a self-contained FlatBuffer.
/// The returned vector owns the bytes; safe to move into an async write.
std::vector<std::uint8_t> SerializeEntity(const snapshot::EntitySnapshot& dto);

/// Verify and deserialize. Returns std::nullopt on malformed input.
std::optional<snapshot::EntitySnapshot> DeserializeEntity(std::span<const std::uint8_t> data);

}  // namespace snapshot_wire

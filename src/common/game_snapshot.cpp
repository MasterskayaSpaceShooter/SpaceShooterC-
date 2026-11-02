#include "game_snapshot.h"

#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "config.h"
#include "entity_snapshot_serder.h"

namespace snapshot {
namespace {

class ByteWriter {
public:
    void writeU32(std::uint32_t value) {
        for (int shift = 24; shift >= 0; shift -= 8) {
            bytes_.push_back(static_cast<std::uint8_t>(value >> shift));
        }
    }

    void writeBytes(std::span<const std::uint8_t> bytes) {
        bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
    }

    std::vector<std::uint8_t> takeBytes() && {
        return std::move(bytes_);
    }

private:
    std::vector<std::uint8_t> bytes_;
};

class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    std::uint32_t readU32() {
        require(sizeof(std::uint32_t));

        std::uint32_t value = 0;
        for (std::size_t i = 0; i < sizeof(value); ++i) {
            value = static_cast<std::uint32_t>((value << 8) | bytes_[offset_++]);
        }
        return value;
    }

    std::span<const std::uint8_t> readBytes(std::size_t count) {
        require(count);
        const auto result = bytes_.subspan(offset_, count);
        offset_ += count;
        return result;
    }

    [[nodiscard]] std::size_t remaining() const noexcept {
        return bytes_.size() - offset_;
    }

    void requireEnd() const {
        if (remaining() != 0) {
            throw std::invalid_argument("GameSnapshot::Deserialize: trailing bytes");
        }
    }

private:
    void require(std::size_t count) const {
        if (count > remaining()) {
            throw std::invalid_argument("GameSnapshot::Deserialize: truncated input");
        }
    }

    std::span<const std::uint8_t> bytes_;
    std::size_t offset_{};
};

}  // namespace

std::vector<std::uint8_t> GameSnapshot::Serialize() const {
    if (snapshots_.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("GameSnapshot::Serialize: too many entities");
    }
    if (leaderboard_.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("GameSnapshot::Serialize: too many leaderboard entries");
    }

    ByteWriter writer;
    writer.writeU32(tick_number_);

    // --- Сериализация лидерборда ---
    writer.writeU32(static_cast<std::uint32_t>(leaderboard_.size()));
    for (const auto& entry : leaderboard_) {
        std::array<std::uint8_t, 32> name_buf{};
        const std::size_t copy_len = std::min(entry.name.size(), name_buf.size());
        std::copy_n(entry.name.begin(), copy_len, name_buf.begin());
        writer.writeBytes(name_buf);
        writer.writeU32(entry.score);
    }

    // --- Сериализация сущностей ---
    writer.writeU32(static_cast<std::uint32_t>(snapshots_.size()));
    for (const auto& entity : snapshots_) {
        const auto bytes = snapshot_wire::SerializeEntity(entity);
        if (bytes.size() > std::numeric_limits<std::uint32_t>::max()) {
            throw std::length_error("GameSnapshot::Serialize: entity payload is too large");
        }
        writer.writeU32(static_cast<std::uint32_t>(bytes.size()));
        writer.writeBytes(bytes);
    }

    return std::move(writer).takeBytes();
}

GameSnapshot GameSnapshot::Deserialize(const std::vector<std::uint8_t>& buffer) {
    ByteReader reader{buffer};
    const std::uint32_t tick_number = reader.readU32();

    // --- Десериализация лидерборда ---
    const std::uint32_t leaderboard_count = reader.readU32();
    constexpr std::size_t kMinLeaderboardEntrySize = 32 + sizeof(std::uint32_t);
    if (leaderboard_count > world::kMaxLeaderboardSize ||
        leaderboard_count > reader.remaining() / kMinLeaderboardEntrySize) {
        throw std::invalid_argument("GameSnapshot::Deserialize: impossible leaderboard count");
    }
    std::vector<LeaderboardEntry> leaderboard;
    leaderboard.reserve(leaderboard_count);
    for (std::uint32_t i = 0; i < leaderboard_count; ++i) {
        const auto name_bytes = reader.readBytes(32);
        std::string_view name_view(reinterpret_cast<const char*>(name_bytes.data()), name_bytes.size());
        if (auto idx = name_view.find('\0'); idx != std::string_view::npos) {
            name_view = name_view.substr(0, idx);
        }
        const std::uint32_t score = reader.readU32();
        leaderboard.push_back({std::string(name_view), score});
    }

    // --- Десериализация сущностей ---
    const std::uint32_t entity_count = reader.readU32();
    if (entity_count > reader.remaining() / sizeof(std::uint32_t)) {
        throw std::invalid_argument("GameSnapshot::Deserialize: impossible entity count");
    }

    std::vector<EntitySnapshot> snapshots;
    snapshots.reserve(entity_count);
    for (std::uint32_t i = 0; i < entity_count; ++i) {
        const std::uint32_t payload_size = reader.readU32();
        const auto payload = reader.readBytes(payload_size);
        auto entity = snapshot_wire::DeserializeEntity(payload);
        if (!entity) {
            throw std::invalid_argument("GameSnapshot::Deserialize: invalid entity payload");
        }
        snapshots.push_back(std::move(*entity));
    }

    reader.requireEnd();
    return snapshot::GameSnapshot(std::move(snapshots), std::move(leaderboard), tick_number);
}

}  // namespace snapshot

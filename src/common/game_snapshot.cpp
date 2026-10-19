#include "game_snapshot.h"

#include <bit>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>

namespace snapshot {
namespace {

// Статические проверки, чтобы убедиться, что типы данных
// имеют ожидаемые размеры и свойства.
static_assert(sizeof(float) == sizeof(std::uint32_t));
static_assert(sizeof(math::Scalar) == sizeof(std::uint64_t));
// Стандарт IEEE 754 гарантирует, что float и double (или math::Scalar)
// имеют определенные свойства, такие как представление NaN и бесконечности.
static_assert(std::numeric_limits<float>::is_iec559);
static_assert(std::numeric_limits<math::Scalar>::is_iec559);

constexpr std::size_t kEntityMinimumSize = 53;
// Запись байтов для каждой сущности в GameSnapshot.
// Каждая сущность сериализуется в фиксированный размер,
// что упрощает десериализацию и обеспечивает предсказуемость формата данных.
// 53 байта на сущность, включая:
// 4 (id) + 1 (type) + 8*4 (position + velocity) + 4 (radius) + 4 (rotation_angle) + 4 (score) + 0 (name length)
class ByteWriter {
public:
    // Методы для записи различных типов данных в последовательность байтов.
    void writeU8(std::uint8_t value) {
        bytes_.push_back(value);
    }

    void writeU32(std::uint32_t value) {
        for (int shift = 24; shift >= 0; shift -= 8) {
            bytes_.push_back(static_cast<std::uint8_t>(value >> shift));
        }
    }

    void writeU64(std::uint64_t value) {
        for (int shift = 56; shift >= 0; shift -= 8) {
            bytes_.push_back(static_cast<std::uint8_t>(value >> shift));
        }
    }

    void writeString(const std::string& value) {
        if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
            throw std::length_error("GameSnapshot::Serialize: entity name is too long");
        }

        writeU32(static_cast<std::uint32_t>(value.size()));
        if (!value.empty()) {
            const auto* begin = reinterpret_cast<const std::uint8_t*>(value.data());
            bytes_.insert(bytes_.end(), begin, begin + value.size());
        }
    }

    std::vector<std::uint8_t> takeBytes() && {
        return std::move(bytes_);
    }

private:
    std::vector<std::uint8_t> bytes_;
};

// Чтение байтов для каждой сущности в GameSnapshot.
class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    std::uint8_t readU8() {
        require(1);
        return bytes_[offset_++];
    }

    std::uint32_t readU32() {
        require(sizeof(std::uint32_t));
        std::uint32_t value = 0;
        for (std::size_t i = 0; i < sizeof(value); ++i) {
            value = static_cast<std::uint32_t>((value << 8) | bytes_[offset_++]);
        }
        return value;
    }

    std::uint64_t readU64() {
        require(sizeof(std::uint64_t));
        std::uint64_t value = 0;
        for (std::size_t i = 0; i < sizeof(value); ++i) {
            value = (value << 8) | bytes_[offset_++];
        }
        return value;
    }

    std::string readString() {
        const std::uint32_t length = readU32();
        require(length);
        if (length == 0) {
            return {};
        }

        const auto* begin = reinterpret_cast<const char*>(bytes_.data() + offset_);
        offset_ += length;
        return {begin, length};
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
    // Проверка, что в буфере достаточно байтов для чтения.
    void require(std::size_t count) const {
        if (count > remaining()) {
            throw std::invalid_argument("GameSnapshot::Deserialize: truncated input");
        }
    }

    // Буфер байтов и текущий смещение для чтения.
    std::span<const std::uint8_t> bytes_;
    std::size_t offset_{};
};

// Функции для кодирования и декодирования типа сущности.
std::uint8_t encodeEntityType(EntityType type) {
    switch (type) {
        case EntityType::Ship:
            return 0;
        case EntityType::Projectile:
            return 1;
        case EntityType::Asteroid:
            return 2;
    }
    throw std::invalid_argument("GameSnapshot::Serialize: invalid entity type");
}

// Декодирование типа сущности из байта.
EntityType decodeEntityType(std::uint8_t value) {
    switch (value) {
        case 0:
            return EntityType::Ship;
        case 1:
            return EntityType::Projectile;
        case 2:
            return EntityType::Asteroid;
        default:
            throw std::invalid_argument("GameSnapshot::Deserialize: invalid entity type");
    }
}

// Кодирование и декодирование идентификатора сущности.
EntityId decodeEntityId(std::uint32_t value) {
    return std::bit_cast<EntityId>(value);
}

// Запись данных сущности в байтовый поток.
void writeEntity(ByteWriter& writer, const EntitySnapshot& entity) {
    writer.writeU32(static_cast<std::uint32_t>(entity.getId()));
    writer.writeU8(encodeEntityType(entity.getType()));

    const auto& position = entity.getPosition();
    writer.writeU64(std::bit_cast<std::uint64_t>(position.x));
    writer.writeU64(std::bit_cast<std::uint64_t>(position.y));

    const auto& velocity = entity.getVelocity();
    writer.writeU64(std::bit_cast<std::uint64_t>(velocity.x));
    writer.writeU64(std::bit_cast<std::uint64_t>(velocity.y));

    writer.writeU32(std::bit_cast<std::uint32_t>(entity.getRadius()));
    writer.writeU32(std::bit_cast<std::uint32_t>(entity.getRotationAngle()));
    writer.writeU32(entity.getScore());
    writer.writeString(entity.getName());
}

// Чтение данных сущности из байтового потока.
EntitySnapshot readEntity(ByteReader& reader) {
    const EntityId id = decodeEntityId(reader.readU32());
    const auto type = decodeEntityType(reader.readU8());
    const math::Vector2D position{
        std::bit_cast<math::Scalar>(reader.readU64()),
        std::bit_cast<math::Scalar>(reader.readU64()),
    };
    const math::Vector2D velocity{
        std::bit_cast<math::Scalar>(reader.readU64()),
        std::bit_cast<math::Scalar>(reader.readU64()),
    };
    const float radius = std::bit_cast<float>(reader.readU32());
    const float rotation_angle = std::bit_cast<float>(reader.readU32());
    const std::uint32_t score = reader.readU32();
    std::string name = reader.readString();

    return EntitySnapshot{id, type, position, velocity, radius, rotation_angle, score, std::move(name)};
}

}  // namespace

std::vector<std::uint8_t> GameSnapshot::Serialize() const {
    if (snapshots_.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("GameSnapshot::Serialize: too many entities");
    }

    ByteWriter writer;
    writer.writeU32(tick_number_);
    writer.writeU32(static_cast<std::uint32_t>(snapshots_.size()));
    for (const auto& entity : snapshots_) {
        writeEntity(writer, entity);
    }
    return std::move(writer).takeBytes();
}

GameSnapshot GameSnapshot::Deserialize(const std::vector<std::uint8_t>& buffer) {
    ByteReader reader{buffer};
    const std::uint32_t tick_number = reader.readU32();
    const std::uint32_t entity_count = reader.readU32();

    if (entity_count > reader.remaining() / kEntityMinimumSize) {
        throw std::invalid_argument("GameSnapshot::Deserialize: impossible entity count");
    }

    std::vector<EntitySnapshot> snapshots;
    snapshots.reserve(entity_count);
    for (std::uint32_t i = 0; i < entity_count; ++i) {
        snapshots.push_back(readEntity(reader));
    }
    reader.requireEnd();
    return GameSnapshot{std::move(snapshots), tick_number};
}

}  // namespace snapshot

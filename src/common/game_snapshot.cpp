#include "game_snapshot.h"

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>
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

// Запись байтов для каждой сущности в GameSnapshot.
// Каждая сущность сериализуется в фиксированный размер,
// что упрощает десериализацию и обеспечивает предсказуемость формата данных.
// 81 байт на сущность, включая имя длиной 32 байта.
// 4 (id) + 1 (type) + 8*4 (position + velocity) + 4 (radius) + 4 (rotation_angle) + 4 (score) + 32 (name) = 81
constexpr std::size_t kEntitySize = 4 + 1 + 8 * 4 + 4 + 4 + 4 + EntitySnapshot::kNameSize;
static_assert(kEntitySize == 81, "wire format changed, update kEntitySize");

template <std::unsigned_integral T>
constexpr T toNetworkOrder(T value) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        T result{};
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            result = static_cast<T>((result << 8) | (value & static_cast<T>(0xFF)));
            value >>= 8;
        }
        return result;
    } else {
        static_assert(std::endian::native == std::endian::big,
                      "GameSnapshot requires a little- or big-endian platform");
        return value;
    }
}

class ByteWriter {
public:
    // Методы для записи различных типов данных в последовательность байтов.
    void writeU8(std::uint8_t value) {
        writeAsBytes(value);
    }

    void writeU32(std::uint32_t value) {
        writeAsBytes(toNetworkOrder(value));
    }

    void writeU64(std::uint64_t value) {
        writeAsBytes(toNetworkOrder(value));
    }

    void writeName(const std::array<char, EntitySnapshot::kNameSize>& value) {
        const auto raw = std::as_bytes(std::span{value});
        bytes_.insert(bytes_.end(),
                      reinterpret_cast<const std::uint8_t*>(raw.data()),
                      reinterpret_cast<const std::uint8_t*>(raw.data()) + raw.size());
    }

    std::vector<std::uint8_t> takeBytes() && {
        return std::move(bytes_);
    }

private:
    template <std::unsigned_integral T>
    void writeAsBytes(const T& value) {
        for (const std::byte byte : std::as_bytes(std::span{&value, std::size_t{1}})) {
            bytes_.push_back(std::to_integer<std::uint8_t>(byte));
        }
    }

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

    std::string_view readName() {
        require(EntitySnapshot::kNameSize);
        const auto* begin = reinterpret_cast<const char*>(bytes_.data() + offset_);
        std::size_t length = 0;
        while (length < EntitySnapshot::kNameSize && begin[length] != '\0') {
            ++length;
        }
        for (std::size_t i = length; i < EntitySnapshot::kNameSize; ++i) {
            if (begin[i] != '\0') {
                throw std::invalid_argument("GameSnapshot::Deserialize: invalid name padding");
            }
        }
        offset_ += EntitySnapshot::kNameSize;
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
    writer.writeName(entity.getNameBuffer());
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
    const std::string_view name = reader.readName();

    return EntitySnapshot{id, type, position, velocity, radius, rotation_angle, score, name};
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

    if (entity_count > reader.remaining() / kEntitySize) {
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

#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>

#include "entity_snapshot.h"
#include "entity_snapshot_serder.h"

namespace {

using snapshot::AsteroidSnapshot;
using snapshot::CombatSnapshot;
using snapshot::EntitySnapshot;
using snapshot::HealthSnapshot;
using snapshot::ShipSnapshot;
using snapshot::TransformSnapshot;

// ---------- tunables ----------

constexpr std::size_t kLongNameLength = 4096;
constexpr int kFuzzIterations = 500;
constexpr std::uint32_t kFuzzSeed = 0xC0FFEEu;
constexpr std::size_t kFlatBufferHeaderSize = 8;  // uoffset + file identifier

// ---------- equality helpers ----------

void ExpectVec2Eq(const math::Vector2D& a, const math::Vector2D& b) {
    EXPECT_NEAR(a.x, b.x, 1e-6);
    EXPECT_NEAR(a.y, b.y, 1e-6);
}

void ExpectSnapshotEq(const EntitySnapshot& a, const EntitySnapshot& b) {
    EXPECT_EQ(a.id, b.id);
    EXPECT_EQ(static_cast<int>(a.type), static_cast<int>(b.type));
    EXPECT_NEAR(a.mass, b.mass, 1e-6);
    EXPECT_NEAR(a.radius, b.radius, 1e-6);
    EXPECT_EQ(a.is_destroyed, b.is_destroyed);

    EXPECT_EQ(a.transform.has_value(), b.transform.has_value());
    if (a.transform && b.transform) {
        SCOPED_TRACE("TransformSnapshot");
        ExpectVec2Eq(a.transform->position, b.transform->position);
        ExpectVec2Eq(a.transform->velocity, b.transform->velocity);
        EXPECT_NEAR(a.transform->angle, b.transform->angle, 1e-6);
    }

    EXPECT_EQ(a.ship.has_value(), b.ship.has_value());
    if (a.ship && b.ship) {
        SCOPED_TRACE("ShipSnapshot");
        EXPECT_EQ(a.ship->name, b.ship->name);
        EXPECT_EQ(a.ship->score, b.ship->score);
        EXPECT_NEAR(a.ship->thrust_power, b.ship->thrust_power, 1e-6);
        EXPECT_NEAR(a.ship->drag, b.ship->drag, 1e-6);
        EXPECT_EQ(a.ship->is_thrusting, b.ship->is_thrusting);
    }

    EXPECT_EQ(a.health.has_value(), b.health.has_value());
    if (a.health && b.health) {
        SCOPED_TRACE("HealthSnapshot");
        EXPECT_EQ(a.health->current, b.health->current);
        EXPECT_EQ(a.health->max, b.health->max);
    }

    EXPECT_EQ(a.combat.has_value(), b.combat.has_value());
    if (a.combat && b.combat) {
        SCOPED_TRACE("CombatSnapshot");
        EXPECT_EQ(a.combat->damage, b.combat->damage);
        EXPECT_EQ(a.combat->owner_id, b.combat->owner_id);
    }

    EXPECT_EQ(a.asteroid.has_value(), b.asteroid.has_value());
    if (a.asteroid && b.asteroid) {
        SCOPED_TRACE("AsteroidSnapshot");
        EXPECT_EQ(a.asteroid->score_reward, b.asteroid->score_reward);
    }
}

// ---------- round-trip helper ----------
//
// NOTE: ASSERT_* inside a void helper returns from the *helper*, not the
// calling test. Every test below calls this as the last statement of its
// body, so this is safe. If you ever add code after a call, switch to
// returning std::optional<EntitySnapshot> and ASSERT in the test body.
void AssertRoundTrip(const EntitySnapshot& dto) {
    const auto bytes = snapshot_wire::SerializeEntity(dto);
    ASSERT_FALSE(bytes.empty());
    const auto result = snapshot_wire::DeserializeEntity(bytes);
    ASSERT_TRUE(result.has_value()) << "Deserialize returned nullopt";
    ExpectSnapshotEq(dto, *result);
}

// ---------- fixtures ----------

EntitySnapshot MakeMinimalEntity() {
    EntitySnapshot e;
    e.id = 1;
    e.type = types::EntityType::Ship;
    e.mass = 10.0f;
    e.radius = 1.5f;
    e.is_destroyed = false;
    return e;
}

EntitySnapshot MakeShipEntity() {
    auto e = MakeMinimalEntity();
    e.type = types::EntityType::Ship;
    e.transform = TransformSnapshot{{1.0f, 2.0f}, {0.5f, -0.5f}, 1.57f};
    e.ship = ShipSnapshot{"Enterprise", 9999, 12.5f, 0.25f, true};
    e.health = HealthSnapshot{75, 100};
    e.combat = CombatSnapshot{10, 55};
    return e;
}

EntitySnapshot MakeAsteroidEntity() {
    auto e = MakeMinimalEntity();
    e.id = 123;
    e.type = types::EntityType::Asteroid;
    e.mass = 42.5f;
    e.radius = 3.25f;
    e.is_destroyed = true;
    e.transform = TransformSnapshot{{10.0f, -3.0f}, {0.0f, 0.0f}, 0.0f};
    e.health = HealthSnapshot{0, 50};
    e.asteroid = AsteroidSnapshot{500};
    return e;
}

}  // namespace

// =========================================================================
// Happy-path round trips
// =========================================================================

TEST(EntitySnapshotSerderTest, RoundTripMinimalEntity) {
    AssertRoundTrip(MakeMinimalEntity());
}

TEST(EntitySnapshotSerderTest, RoundTripShipEntity) {
    AssertRoundTrip(MakeShipEntity());
}

TEST(EntitySnapshotSerderTest, RoundTripAsteroidEntity) {
    AssertRoundTrip(MakeAsteroidEntity());
}

TEST(EntitySnapshotSerderTest, RoundTripEntityWithOnlyTransform) {
    auto dto = MakeMinimalEntity();
    dto.transform = TransformSnapshot{{1, 2}, {3, 4}, 0.5f};
    AssertRoundTrip(dto);
}

TEST(EntitySnapshotSerderTest, RoundTripEntityWithOnlyHealth) {
    auto dto = MakeMinimalEntity();
    dto.health = HealthSnapshot{5, 10};
    AssertRoundTrip(dto);
}

TEST(EntitySnapshotSerderTest, RoundTripEmptyShipName) {
    auto dto = MakeMinimalEntity();
    dto.ship = ShipSnapshot{"", 0, 0.0f, 0.0f, false};
    AssertRoundTrip(dto);
}

TEST(EntitySnapshotSerderTest, RoundTripLongShipName) {
    auto dto = MakeMinimalEntity();
    dto.ship = ShipSnapshot{std::string(kLongNameLength, 'x'), 1, 1.0f, 1.0f, true};
    AssertRoundTrip(dto);
}

// =========================================================================
// Malformed input — all produced by mutating SerializeEntity output,
// exercising only the public API.
// =========================================================================

TEST(EntitySnapshotSerderTest, DeserializeEmptyInputReturnsNullopt) {
    const std::vector<std::uint8_t> empty;
    EXPECT_FALSE(snapshot_wire::DeserializeEntity(empty).has_value());
}

TEST(EntitySnapshotSerderTest, DeserializeTooSmallReturnsNullopt) {
    const std::vector<std::uint8_t> tiny(kFlatBufferHeaderSize - 1, 0);
    EXPECT_FALSE(snapshot_wire::DeserializeEntity(tiny).has_value());
}

TEST(EntitySnapshotSerderTest, DeserializeGarbageReturnsNullopt) {
    const std::vector<std::uint8_t> garbage(64, 0xAB);
    EXPECT_FALSE(snapshot_wire::DeserializeEntity(garbage).has_value());
}

TEST(EntitySnapshotSerderTest, DeserializeTruncatedBufferReturnsNullopt) {
    const auto full = snapshot_wire::SerializeEntity(MakeShipEntity());
    ASSERT_GT(full.size(), kFlatBufferHeaderSize);
    std::vector<std::uint8_t> truncated(full.begin(), full.begin() + full.size() / 2);
    EXPECT_FALSE(snapshot_wire::DeserializeEntity(truncated).has_value());
}

TEST(EntitySnapshotSerderTest, DeserializeCorruptedFileIdentifierReturnsNullopt) {
    auto bytes = snapshot_wire::SerializeEntity(MakeMinimalEntity());
    ASSERT_GE(bytes.size(), kFlatBufferHeaderSize);
    bytes[4] ^= 0xFF;  // first byte of file identifier (bytes 4..7)
    EXPECT_FALSE(snapshot_wire::DeserializeEntity(bytes).has_value());
}

TEST(EntitySnapshotSerderTest, DeserializeCorruptedRootOffsetReturnsNullopt) {
    auto bytes = snapshot_wire::SerializeEntity(MakeMinimalEntity());
    ASSERT_GE(bytes.size(), kFlatBufferHeaderSize);
    // Corrupt root uoffset (bytes 0..3). The verifier must reject a
    // bogus offset that doesn't land on a valid table.
    bytes[0] ^= 0xFF;
    bytes[1] ^= 0xFF;
    bytes[2] ^= 0xFF;
    bytes[3] ^= 0xFF;
    EXPECT_FALSE(snapshot_wire::DeserializeEntity(bytes).has_value());
}

// =========================================================================
// Robustness smoke test: bit-flip fuzzing through the public API.
// Not a proof, but catches crashes on malformed-but-verifier-passing input
// that round-trip tests can't reach. A SIGSEGV here is the failure signal.
// =========================================================================

TEST(EntitySnapshotSerderTest, BitFlipFuzzDoesNotCrash) {
    const auto seed_buffer = snapshot_wire::SerializeEntity(MakeShipEntity());
    ASSERT_GT(seed_buffer.size(), kFlatBufferHeaderSize);

    std::mt19937 rng(kFuzzSeed);
    std::uniform_int_distribution<std::size_t> pos_dist(0, seed_buffer.size() - 1);
    std::uniform_int_distribution<int> bit_dist(0, 7);
    std::uniform_int_distribution<int> flips_dist(1, 4);

    for (int i = 0; i < kFuzzIterations; ++i) {
        auto bytes = seed_buffer;
        const int flips = flips_dist(rng);
        for (int f = 0; f < flips; ++f) {
            const auto p = pos_dist(rng);
            bytes[p] ^= static_cast<std::uint8_t>(1u << bit_dist(rng));
        }
        // Contract: never crash. We deliberately don't assert has_value(),
        // because most single-bit flips will produce rejectable buffers.
        const auto result = snapshot_wire::DeserializeEntity(bytes);
        (void)result;
    }
}

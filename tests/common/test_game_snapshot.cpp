#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

#include "game_snapshot.h"

namespace {

using snapshot::AsteroidSnapshot;
using snapshot::CombatSnapshot;
using snapshot::EntitySnapshot;
using snapshot::GameSnapshot;
using snapshot::HealthSnapshot;
using snapshot::ShipSnapshot;
using snapshot::TransformSnapshot;
using types::EntityId;
using types::EntityType;
using Bytes = std::vector<std::uint8_t>;

EntitySnapshot MakeShip() {
    EntitySnapshot entity{};
    entity.id = EntityId{42};
    entity.type = EntityType::Ship;
    entity.mass = 12.5;
    entity.radius = 3.25;
    entity.is_destroyed = false;
    entity.transform = TransformSnapshot{{123.25, -456.5}, {-0.125, 2.75}, 1.25};
    entity.ship = ShipSnapshot{"player", 987654U, 7.5, 0.125, true, 4};
    entity.health = HealthSnapshot{75, 100};
    entity.combat = CombatSnapshot{10, EntityId{7}};
    return entity;
}

EntitySnapshot MakeAsteroid() {
    EntitySnapshot entity{};
    entity.id = EntityId{7};
    entity.type = EntityType::Asteroid;
    entity.mass = 100.0;
    entity.radius = 8.0;
    entity.is_destroyed = true;
    entity.transform = TransformSnapshot{{-0.0, 0.0}, {0.0, -0.0}, 0.5};
    entity.health = HealthSnapshot{0, 50};
    entity.asteroid = AsteroidSnapshot{500};
    return entity;
}

void ExpectEntityEq(const EntitySnapshot& actual, const EntitySnapshot& expected) {
    EXPECT_EQ(actual.id, expected.id);
    EXPECT_EQ(actual.type, expected.type);
    EXPECT_DOUBLE_EQ(actual.mass, expected.mass);
    EXPECT_DOUBLE_EQ(actual.radius, expected.radius);
    EXPECT_EQ(actual.is_destroyed, expected.is_destroyed);

    ASSERT_EQ(actual.transform.has_value(), expected.transform.has_value());
    if (actual.transform) {
        EXPECT_DOUBLE_EQ(actual.transform->position.x, expected.transform->position.x);
        EXPECT_DOUBLE_EQ(actual.transform->position.y, expected.transform->position.y);
        EXPECT_DOUBLE_EQ(actual.transform->velocity.x, expected.transform->velocity.x);
        EXPECT_DOUBLE_EQ(actual.transform->velocity.y, expected.transform->velocity.y);
        EXPECT_DOUBLE_EQ(actual.transform->angle, expected.transform->angle);
    }

    ASSERT_EQ(actual.ship.has_value(), expected.ship.has_value());
    if (actual.ship) {
        EXPECT_EQ(actual.ship->name, expected.ship->name);
        EXPECT_EQ(actual.ship->score, expected.ship->score);
        EXPECT_DOUBLE_EQ(actual.ship->thrust_power, expected.ship->thrust_power);
        EXPECT_DOUBLE_EQ(actual.ship->drag, expected.ship->drag);
        EXPECT_EQ(actual.ship->is_thrusting, expected.ship->is_thrusting);
        EXPECT_EQ(actual.ship->frags, expected.ship->frags);
    }

    ASSERT_EQ(actual.health.has_value(), expected.health.has_value());
    if (actual.health) {
        EXPECT_EQ(actual.health->current, expected.health->current);
        EXPECT_EQ(actual.health->max, expected.health->max);
    }

    ASSERT_EQ(actual.combat.has_value(), expected.combat.has_value());
    if (actual.combat) {
        EXPECT_EQ(actual.combat->damage, expected.combat->damage);
        EXPECT_EQ(actual.combat->owner_id, expected.combat->owner_id);
    }

    ASSERT_EQ(actual.asteroid.has_value(), expected.asteroid.has_value());
    if (actual.asteroid) {
        EXPECT_EQ(actual.asteroid->score_reward, expected.asteroid->score_reward);
    }
}

TEST(GameSnapshotTest, RoundTripsAllEntityFields) {
    const std::vector<EntitySnapshot> entities{MakeShip(), MakeAsteroid()};
    GameSnapshot original{entities, {}, 0x12345678U};

    const auto bytes = original.Serialize();
    const auto decoded = GameSnapshot::Deserialize(bytes);

    EXPECT_EQ(decoded.getTickNumber(), original.getTickNumber());
    ASSERT_EQ(decoded.getSnapshots().size(), entities.size());
    for (std::size_t i = 0; i < entities.size(); ++i) {
        ExpectEntityEq(decoded.getSnapshots()[i], entities[i]);
    }
}

TEST(GameSnapshotTest, EncodesHeaderInNetworkByteOrder) {
    GameSnapshot snapshot{{}, {}, 0x12345678U};

    EXPECT_EQ(snapshot.Serialize(), (Bytes{0x12, 0x34, 0x56, 0x78, 0, 0, 0, 0}));
}

TEST(GameSnapshotTest, RoundTripsEmptySnapshot) {
    GameSnapshot original{{}, {}, 19U};

    const auto decoded = GameSnapshot::Deserialize(original.Serialize());

    EXPECT_EQ(decoded.getTickNumber(), 19U);
    EXPECT_TRUE(decoded.getSnapshots().empty());
}

TEST(GameSnapshotTest, SupportsLongNamesAndEmbeddedNullBytes) {
    auto entity = MakeShip();
    entity.ship->name = std::string(4096, 'n');
    entity.ship->name[10] = '\0';
    const GameSnapshot original{{entity}, {}, 0U};

    const auto decoded = GameSnapshot::Deserialize(original.Serialize());

    ASSERT_EQ(decoded.getSnapshots().size(), 1U);
    EXPECT_EQ(decoded.getSnapshots()[0].ship->name, entity.ship->name);
}

TEST(GameSnapshotTest, RejectsTruncatedInput) {
    EXPECT_THROW((void)GameSnapshot::Deserialize({}), std::invalid_argument);

    const GameSnapshot original{{MakeShip()}, {}, 1U};
    auto bytes = original.Serialize();
    bytes.pop_back();

    EXPECT_THROW((void)GameSnapshot::Deserialize(bytes), std::invalid_argument);
}

TEST(GameSnapshotTest, RejectsImpossibleEntityCount) {
    const Bytes bytes{0, 0, 0, 0, 0, 0, 0, 1};

    EXPECT_THROW((void)GameSnapshot::Deserialize(bytes), std::invalid_argument);
}

TEST(GameSnapshotTest, RejectsInvalidEntityPayload) {
    auto bytes = GameSnapshot{{MakeShip()}, {}, 0U}.Serialize();
    ASSERT_GT(bytes.size(), 16U);
    bytes[16] ^= 0xFF;

    EXPECT_THROW((void)GameSnapshot::Deserialize(bytes), std::invalid_argument);
}

TEST(GameSnapshotTest, RejectsTrailingBytes) {
    auto bytes = GameSnapshot{{}, {}, 0U}.Serialize();
    bytes.push_back(0);

    EXPECT_THROW((void)GameSnapshot::Deserialize(bytes), std::invalid_argument);
}

}  // namespace

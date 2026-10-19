#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

#include "game_snapshot.h"

namespace {

using snapshot::EntitySnapshot;
using snapshot::EntityType;
using snapshot::GameSnapshot;
using Bytes = std::vector<std::uint8_t>;

TEST(GameSnapshotTest, RoundTripsAllEntityFields) {
    const EntitySnapshot ship{
        -42,
        EntityType::Ship,
        {123.25, -456.5},
        {-0.125, 2.75},
        17.5F,
        -1.25F,
        987654U,
        std::string{"player\0one", 10},
    };
    const EntitySnapshot asteroid{
        7,
        EntityType::Asteroid,
        {-0.0, 0.0},
        {0.0, -0.0},
        3.0F,
        0.5F,
        0U,
        "",
    };
    GameSnapshot original{{ship, asteroid}, 0x12345678U};

    const auto bytes = original.Serialize();
    auto decoded = GameSnapshot::Deserialize(bytes);

    EXPECT_EQ(decoded.getTickNumber(), original.getTickNumber());
    ASSERT_EQ(decoded.getSnapshots().size(), 2U);

    const auto& decoded_ship = decoded.getSnapshots()[0];
    EXPECT_EQ(decoded_ship.getId(), ship.getId());
    EXPECT_EQ(decoded_ship.getType(), ship.getType());
    EXPECT_EQ(decoded_ship.getPosition().x, ship.getPosition().x);
    EXPECT_EQ(decoded_ship.getPosition().y, ship.getPosition().y);
    EXPECT_EQ(decoded_ship.getVelocity().x, ship.getVelocity().x);
    EXPECT_EQ(decoded_ship.getVelocity().y, ship.getVelocity().y);
    EXPECT_EQ(decoded_ship.getRadius(), ship.getRadius());
    EXPECT_EQ(decoded_ship.getRotationAngle(), ship.getRotationAngle());
    EXPECT_EQ(decoded_ship.getScore(), ship.getScore());
    EXPECT_EQ(decoded_ship.getName(), ship.getName());

    const auto& decoded_asteroid = decoded.getSnapshots()[1];
    EXPECT_EQ(decoded_asteroid.getId(), asteroid.getId());
    EXPECT_EQ(decoded_asteroid.getType(), asteroid.getType());
    EXPECT_EQ(decoded_asteroid.getPosition().x, asteroid.getPosition().x);
    EXPECT_EQ(decoded_asteroid.getPosition().y, asteroid.getPosition().y);
    EXPECT_EQ(decoded_asteroid.getVelocity().x, asteroid.getVelocity().x);
    EXPECT_EQ(decoded_asteroid.getVelocity().y, asteroid.getVelocity().y);
    EXPECT_EQ(decoded_asteroid.getRadius(), asteroid.getRadius());
    EXPECT_EQ(decoded_asteroid.getRotationAngle(), asteroid.getRotationAngle());
    EXPECT_EQ(decoded_asteroid.getScore(), asteroid.getScore());
    EXPECT_EQ(decoded_asteroid.getName(), asteroid.getName());
}

TEST(GameSnapshotTest, EncodesHeaderInNetworkByteOrder) {
    GameSnapshot snapshot{{}, 0x12345678U};

    const auto bytes = snapshot.Serialize();

    EXPECT_EQ(bytes, (Bytes{0x12, 0x34, 0x56, 0x78, 0x00, 0x00, 0x00, 0x00}));
}

TEST(GameSnapshotTest, RoundTripsEmptySnapshot) {
    GameSnapshot original{{}, 19U};

    auto decoded = GameSnapshot::Deserialize(original.Serialize());

    EXPECT_EQ(decoded.getTickNumber(), 19U);
    EXPECT_TRUE(decoded.getSnapshots().empty());
}

TEST(GameSnapshotTest, RejectsTruncatedInput) {
    EXPECT_THROW((void)GameSnapshot::Deserialize({}), std::invalid_argument);

    GameSnapshot snapshot{{}, 1U};
    auto bytes = snapshot.Serialize();
    bytes.pop_back();
    EXPECT_THROW((void)GameSnapshot::Deserialize(bytes), std::invalid_argument);

    GameSnapshot with_entity{{EntitySnapshot{1, EntityType::Ship, {}, {}, 1.0F, 0.0F, 0, "name"}}, 1U};
    bytes = with_entity.Serialize();
    bytes.pop_back();
    EXPECT_THROW((void)GameSnapshot::Deserialize(bytes), std::invalid_argument);
}

TEST(GameSnapshotTest, RejectsImpossibleEntityCount) {
    const Bytes bytes{0, 0, 0, 0, 0, 0, 0, 1};

    EXPECT_THROW((void)GameSnapshot::Deserialize(bytes), std::invalid_argument);
}

TEST(GameSnapshotTest, RejectsUnknownEntityType) {
    GameSnapshot snapshot{{EntitySnapshot{1, EntityType::Ship, {}, {}, 1.0F, 0.0F, 0, ""}}, 0U};
    auto bytes = snapshot.Serialize();
    bytes[12] = 0xFF;

    EXPECT_THROW((void)GameSnapshot::Deserialize(bytes), std::invalid_argument);
}

TEST(GameSnapshotTest, RejectsTrailingBytes) {
    GameSnapshot snapshot{{}, 0U};
    auto bytes = snapshot.Serialize();
    bytes.push_back(0);

    EXPECT_THROW((void)GameSnapshot::Deserialize(bytes), std::invalid_argument);
}

}  // namespace

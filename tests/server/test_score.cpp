#include <cstdint>
#include <gtest/gtest.h>
#include <limits>

#include "score_utils.h"

namespace {

TEST(ScoreTest, AddScoreIncreasesScore) {
    domain::ShipComponent ship{};
    ship.score = 100;

    domain::addScore(ship, 50);

    EXPECT_EQ(ship.score, 150);
}

TEST(ScoreTest, AddZeroDoesNotChangeScore) {
    domain::ShipComponent ship{};
    ship.score = 100;

    domain::addScore(ship, 0);

    EXPECT_EQ(ship.score, 100);
}

TEST(ScoreTest, AddScoreDoesNotOverflow) {
    domain::ShipComponent ship{};
    ship.score = std::numeric_limits<std::uint32_t>::max() - 5;

    domain::addScore(ship, 10);

    EXPECT_EQ(ship.score, std::numeric_limits<std::uint32_t>::max());
}

TEST(ScoreTest, SubtractScoreDecreasesScore) {
    domain::ShipComponent ship{};
    ship.score = 100;

    domain::subtractScore(ship, 30);

    EXPECT_EQ(ship.score, 70);
}

TEST(ScoreTest, SubtractExactScoreSetsScoreToZero) {
    domain::ShipComponent ship{};
    ship.score = 50;

    domain::subtractScore(ship, 50);

    EXPECT_EQ(ship.score, 0);
}

TEST(ScoreTest, SubtractExcessivePenaltySetsScoreToZero) {
    domain::ShipComponent ship{};
    ship.score = 20;

    domain::subtractScore(ship, 30);

    EXPECT_EQ(ship.score, 0);
}

TEST(ScoreTest, SubtractZeroDoesNotChangeScore) {
    domain::ShipComponent ship{};
    ship.score = 100;

    domain::subtractScore(ship, 0);

    EXPECT_EQ(ship.score, 100);
}

}  // namespace

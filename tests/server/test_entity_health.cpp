
#include <gtest/gtest.h>

#include "health_utils.h"

namespace {

TEST(HealthComponentTest, ApplyDamageReducesHealth) {
    domain::HealthComponent health{100, 100};

    domain::applyDamage(health, 25);

    EXPECT_EQ(health.current, 75);
}

TEST(HealthComponentTest, ApplyDamageDoesNotMakeHealthNegative) {
    domain::HealthComponent health{20, 100};

    domain::applyDamage(health, 25);

    EXPECT_EQ(health.current, 0);
}

TEST(HealthComponentTest, ApplyDamageCanReduceHealthToZero) {
    domain::HealthComponent health{25, 100};

    domain::applyDamage(health, 25);

    EXPECT_EQ(health.current, 0);
}

TEST(HealthComponentTest, ZeroDamageDoesNotChangeHealth) {
    domain::HealthComponent health{75, 100};

    domain::applyDamage(health, 0);

    EXPECT_EQ(health.current, 75);
}

TEST(HealthComponentTest, RestoreHealthIncreasesHealth) {
    domain::HealthComponent health{50, 100};

    domain::restoreHealth(health, 25);

    EXPECT_EQ(health.current, 75);
}

TEST(HealthComponentTest, RestoreHealthDoesNotExceedMaximum) {
    domain::HealthComponent health{90, 100};

    domain::restoreHealth(health, 25);

    EXPECT_EQ(health.current, 100);
}

TEST(HealthComponentTest, RestoreHealthCanRestoreToMaximum) {
    domain::HealthComponent health{75, 100};

    domain::restoreHealth(health, 25);

    EXPECT_EQ(health.current, 100);
}

TEST(HealthComponentTest, AliveEntityHasPositiveHealth) {
    const domain::HealthComponent health{1, 100};

    EXPECT_TRUE(domain::isAlive(health));
}

TEST(HealthComponentTest, ZeroHealthMeansDead) {
    const domain::HealthComponent health{0, 100};

    EXPECT_FALSE(domain::isAlive(health));
}

}  // namespace

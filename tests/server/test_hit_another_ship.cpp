#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "world_instance.h"

class TwoShipsShootTestAccessor {
public:
    static domain::EntityPool& entities(WorldInstance& w) noexcept {
        return w.entities_;
    }

    static void onCollisionBulletShip(WorldInstance& w, domain::Entity& bullet, domain::Entity& ship) {
        w.onCollisionBulletShip(bullet, ship);
    }
};

namespace {

struct WorldFixture {
    boost::asio::io_context io;
    std::shared_ptr<events::EventBus> bus = events::EventBus::create();
    std::shared_ptr<WorldInstance> world;
    WorldFixture() {
        world = WorldInstance::create(io, bus, 60.0);
    }
};

domain::Entity* spawnShip(WorldInstance& world, const std::string& name) {
    auto& pool = TwoShipsShootTestAccessor::entities(world);
    auto* e = pool.acquire();
    e->type = types::EntityType::Ship;
    e->mass = world::kShipDefaultMass;
    e->radius = world::kShipDefaultRadius;
    e->transform = domain::TransformComponent{};
    domain::ShipComponent ship;
    ship.name.fill('\0');
    std::copy_n(name.data(), std::min(name.size(), ship.name.size() - 1), ship.name.begin());
    e->ship = ship;
    e->health = domain::HealthComponent{world::kShipDefaultHealth, world::kShipDefaultHealth};
    e->combat = domain::CombatComponent{world::kShipDefaultDamage, e->id};
    return e;
}

domain::Entity* spawnBulletFrom(WorldInstance& world, const domain::Entity& shooter) {
    auto& pool = TwoShipsShootTestAccessor::entities(world);
    auto* b = pool.acquire();
    b->type = types::EntityType::Bullet;
    b->radius = world::kBulletRadius;
    b->transform = domain::TransformComponent{shooter.transform->position, {0.0, 0.0}, 0.0};
    b->combat = domain::CombatComponent{world::kBulletDamage, shooter.id};
    return b;
}

}  // namespace

TEST(TwoShipsShootTest, ShooterGetsPointsAndTargetLosesHealth) {
    WorldFixture f;

    auto* shooter = spawnShip(*f.world, "shooter");
    auto* target = spawnShip(*f.world, "target");

    const auto shooter_score_0 = shooter->ship->score;
    const auto target_hp_0 = target->health->current;
    const auto target_score_0 = target->ship->score;

    auto* bullet = spawnBulletFrom(*f.world, *shooter);

    TwoShipsShootTestAccessor::onCollisionBulletShip(*f.world, *bullet, *target);

    EXPECT_GT(shooter->ship->score, shooter_score_0);
    EXPECT_EQ(shooter->ship->score, shooter_score_0 + 5);

    EXPECT_LT(target->health->current, target_hp_0);
    EXPECT_EQ(target->health->current, target_hp_0 - world::kBulletDamage);

    EXPECT_EQ(target->ship->score, target_score_0);

    EXPECT_TRUE(bullet->isDestroyed);
    EXPECT_FALSE(target->isDestroyed);
}

TEST(TwoShipsShootTest, ShooterGetsKillBonusWhenTargetDies) {
    WorldFixture f;

    auto* shooter = spawnShip(*f.world, "shooter");
    auto* target = spawnShip(*f.world, "target");
    target->health->current = world::kBulletDamage;

    auto* bullet = spawnBulletFrom(*f.world, *shooter);
    TwoShipsShootTestAccessor::onCollisionBulletShip(*f.world, *bullet, *target);

    EXPECT_EQ(shooter->ship->score, 50);
    EXPECT_EQ(target->health->current, 0);
    EXPECT_TRUE(target->isDestroyed);
    EXPECT_EQ(target->ship->score, 0);
}

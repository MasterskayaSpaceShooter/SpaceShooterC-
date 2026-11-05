#include <gtest/gtest.h>

#include "client_world.h"
#include "death_screen.h"
#include "game_snapshot.h"
#include "null_renderer.h"

namespace {

class MockRenderer : public client::IRenderer {
public:
    std::vector<std::string> texts;
    void renderWelcomeWindow(const std::string&) override {}
    void renderGame(const client::ClientWorld&) override {}
    void renderDeathScreen(std::uint32_t) override {}
    void drawText(const std::string& text, int, int) override {
        texts.push_back(text);
    }
};

}  // namespace

TEST(DeathScreenTest, ActivatesWhenPlayerDiesAndDeactivatesOnRespawn) {
    client::ClientWorld world;
    client::DeathScreen screen;
    types::EntityId my_id{1};

    // 1. Игрок жив (есть в entities_ и is_destroyed = false)
    snapshot::EntitySnapshot alive_ship{};
    alive_ship.id = my_id;
    alive_ship.is_destroyed = false;

    snapshot::GameSnapshot snapshot_alive({alive_ship}, {}, 1);
    world.updateSnapshot(snapshot_alive, my_id);
    world.updateDeathScreen(screen);
    EXPECT_FALSE(screen.isActive());  // Экран скрыт

    // 2. Игрок умирает (is_destroyed = true)
    snapshot::EntitySnapshot dead_ship{};
    dead_ship.id = my_id;
    dead_ship.is_destroyed = true;

    snapshot::GameSnapshot snapshot_dead({dead_ship}, {}, 2);
    world.updateSnapshot(snapshot_dead, my_id);
    world.updateDeathScreen(screen);
    EXPECT_TRUE(screen.isActive());  // Экран показан

    // 3. Игрок возрождается (снова is_destroyed = false)
    world.updateSnapshot(snapshot_alive, my_id);
    world.updateDeathScreen(screen);
    EXPECT_FALSE(screen.isActive());  // Экран скрыт
}

TEST(DeathScreenTest, InitiallyInactive) {
    client::DeathScreen screen;
    EXPECT_FALSE(screen.isActive());
}

TEST(DeathScreenTest, ActivatesAndDeactivates) {
    client::DeathScreen screen;
    screen.setActive(true);
    EXPECT_TRUE(screen.isActive());
    screen.setActive(false);
    EXPECT_FALSE(screen.isActive());
}

TEST(DeathScreenTest, RendersOnlyWhenActive) {
    client::DeathScreen screen;
    MockRenderer renderer;

    // Не активен — ничего не рисуем
    screen.render(renderer, 100, 5);
    EXPECT_TRUE(renderer.texts.empty());

    // Активен — рисуем
    screen.setActive(true);
    screen.render(renderer, 100, 5);
    EXPECT_EQ(renderer.texts.size(), 2);
    EXPECT_EQ(renderer.texts[0], "YOU DIED");
    EXPECT_EQ(renderer.texts[1], "Score: 100 | Place: 5");
}

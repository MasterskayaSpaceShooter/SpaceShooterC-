#include "leaderboard_view.h"

#include <format>
#include <string>

namespace client {
void LeaderboardView::render(IRenderer& renderer, const ClientWorld& world) {
    const auto& top = world.getLeaderboard();

    int current_y = 50;
    for (size_t i = 0; i < top.size(); ++i) {
        std::string text = std::format("{}. {} - {}", i + 1, top[i].name, top[i].score);
        renderer.drawText(text, 500, current_y);  // Рисуем справа
        current_y += 25;
    }
}
}  // namespace client

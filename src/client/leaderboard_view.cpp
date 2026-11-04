#include "leaderboard_view.h"

#include <array>
#include <format>
#include <string>

namespace client {

void LeaderboardView::render(IRenderer& renderer, const ClientWorld& world) {
    const auto& top = world.getLeaderboard();

    int current_y = kStartY;
    std::array<char, 128> buffer;

    for (size_t i = 0; i < top.size(); ++i) {
        auto [out, count] =
            std::format_to_n(buffer.data(), buffer.size() - 1, "{}. {} - {}", i + 1, top[i].name, top[i].score);
        *out = '\0';

        renderer.drawText(std::string(buffer.data(), count), kStartX, current_y);
        current_y += kLineHeight;
    }
}

}  // namespace client

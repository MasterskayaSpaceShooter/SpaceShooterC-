#include "death_screen.h"

#include <format>
#include <string>

namespace client {

void DeathScreen::render(IRenderer& renderer, int finalScore, int place) {
    if (!is_active_) {
        return;
    }

    // TODO: вынести координаты в константы класса (как в LeaderboardView)
    const std::string title = "YOU DIED";
    const std::string stats = std::format("Score: {} | Place: {}", finalScore, place);

    renderer.drawText(title, 400, 300);
    renderer.drawText(stats, 400, 330);
}

}  // namespace client

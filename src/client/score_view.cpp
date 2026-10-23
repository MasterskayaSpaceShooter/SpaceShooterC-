#include "score_view.h"

#include <string>
#include <format>

namespace client {

void ScoreView::render(IRenderer& renderer, const ClientWorld& world) {
    // Берем ID игрока
    const auto my_id = world.myPlayerId();
    const auto& entities = world.getEntities();

    std::uint32_t current_score = 0;

    const auto it = entities.find(my_id);
    // Если нашли игрока в мире
    if (it != entities.end()) {
        const auto& entity = it->second;
        // Проверяем, что у сущности есть компонент корабля
        if (entity.ship.has_value()) {
            current_score = entity.ship->score;
        }
    }

    // Кеширование + std::format
    if (!is_initialized_ || current_score != last_score_) {
        cached_text_ = std::format("Score: {}", current_score);
        last_score_ = current_score;
        is_initialized_ = true;
    }

    // TODO: make class parameters, change on window size change
    renderer.drawText(score_text, 10, 10);
}

}  // namespace client

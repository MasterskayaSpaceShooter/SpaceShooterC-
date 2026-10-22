#include "score_view.h"

#include <string>

namespace client {

void ScoreView::render(IRenderer& renderer, const ClientWorld& world) {
    // Берем ID игрока
    auto my_id = world.myPlayerId();
    const auto& entities = world.getEntities();
    auto it = entities.find(my_id);

    std::uint32_t current_score = 0;

    // Если нашли игрока в мире
    if (it != entities.end()) {
        const auto& entity = it->second;
        // Проверяем, что у сущности есть компонент корабля
        if (entity.ship.has_value()) {
            current_score = entity.ship->score;
        }
    }

    // Формируем строку и отрисовываем
    std::string score_text = "Score: " + std::to_string(current_score);

    // Выводим в левом верхнем углу (10, 10)
    renderer.drawText(score_text, 10, 10);
}

}  // namespace client

#pragma once

#include "client_world.h"
#include "renderer.h"

namespace client {

/**
 * @brief Отвечает за отображение счёта текущего игрока.
 */
class ScoreView {
public:
    ScoreView() = default;

    /**
     * @brief Рендерит счёт игрока.
     * @param renderer Ссылка на интерфейс рендеринга.
     * @param world Ссылка на локальный мир клиента.
     */
    void render(IRenderer& renderer, const ClientWorld& world);
};

}  // namespace client

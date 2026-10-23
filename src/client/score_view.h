#pragma once

#include <cstdint>
#include <string>

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

private:
    std::uint32_t last_score_{0};
    std::string cached_text_{};
    bool is_initialized_{false};
};

}  // namespace client

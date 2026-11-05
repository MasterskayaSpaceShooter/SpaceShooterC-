#pragma once

#include "renderer.h"

namespace client {

/**
 * @brief Отвечает за отображение экрана смерти игрока.
 */
class DeathScreen {
public:
    DeathScreen() = default;

    /**
     * @brief Возвращает, активен ли экран смерти.
     */
    [[nodiscard]] bool isActive() const noexcept {
        return is_active_;
    }

    /**
     * @brief Активирует или деактивирует экран смерти.
     * @param active true — показать экран, false — скрыть.
     */
    void setActive(bool active) noexcept {
        is_active_ = active;
    }

    /**
     * @brief Рендерит экран смерти.
     * @param renderer Ссылка на интерфейс рендеринга.
     * @param finalScore Финальный счёт игрока.
     * @param place Место игрока в таблице лидеров.
     */
    void render(IRenderer& renderer, int finalScore, int place);

private:
    static constexpr int kStartX = 400;
    static constexpr int kTitleY = 300;
    static constexpr int kStatsY = 330;
    bool is_active_{false};
};

}  // namespace client

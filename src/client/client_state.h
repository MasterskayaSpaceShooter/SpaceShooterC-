#pragma once

namespace client {

/**
 * @brief Экран/состояние клиента (UI-машина).
 * @details Определяет, что сейчас показывается игроку:
 *          - WelcomeWindow — экран приветствия/ввода имени.
 *          - InputScreen   — основной игровой экран.
 *          - DeathScreen   — экран смерти с результатом.
 */
enum class ClientState {
    WelcomeWindow,
    InputScreen,
    DeathScreen,
};
}  // namespace client

#pragma once

#include <cstdint>
#include <string>

#include "client_world.h"

namespace client {

/**
 * @brief Интерфейс рендера клиента.
 * @details Позволяет подменять реализацию: реальный рендер (SDL/SFML/OpenGL)
 *          или заглушка (NullRenderer) в тестах.
 */
class IRenderer {
public:
    virtual ~IRenderer() = default;

    /// Экран приветствия/ввода имени.
    virtual void renderWelcomeWindow(const std::string& msg) = 0;

    /// Основной игровой экран.
    virtual void renderGame(const ClientWorld& world) = 0;

    /// Экран смерти с финальным счётом.
    virtual void renderDeathScreen(std::uint32_t score) = 0;
};
}  // namespace client

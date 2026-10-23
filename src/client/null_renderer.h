#pragma once

#include "renderer.h"

namespace client {

/**
 * @brief Заглушка IRenderer — ничего не делает.
 * @details Используется в тестах и на ранних этапах, пока нет реального рендера.
 */
class NullRenderer final : public IRenderer {
public:
    void renderWelcomeWindow(const std::string& /*msg*/) override {}
    void renderGame(const ClientWorld& /*world*/) override {}
    void renderDeathScreen(std::uint32_t /*score*/) override {}
    void drawText(const std::string& text, int x, int y) override {
        (void)text;
        (void)x;
        (void)y;
    }
};
}  // namespace client

#pragma once

#include "renderer.h"

namespace client {

class NullRenderer : public IRenderer {
public:
    void renderWelcomeWindow(const std::string&) override {}
    void renderGame(const ClientWorld&) override {}
    void renderDeathScreen(std::uint32_t) override {}
    void drawText(const std::string&, int, int) override {}
};

}  // namespace client

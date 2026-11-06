#pragma once

#include <cstdint>
#include <string>

namespace client {

class ClientWorld;

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual void renderWelcomeWindow(const std::string& player_name) = 0;
    virtual void renderGame(const ClientWorld& world) = 0;
    virtual void renderDeathScreen(std::uint32_t final_score) = 0;
    virtual void drawText(const std::string& text, int x, int y) = 0;
};

}  // namespace client

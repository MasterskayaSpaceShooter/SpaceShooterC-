#pragma once
#include "client_world.h"
#include "renderer.h"

namespace client {
class LeaderboardView {
public:
    LeaderboardView() = default;
    void render(IRenderer& renderer, const ClientWorld& world);

private:
    static constexpr int kStartX = 500;     // X-координата таблицы
    static constexpr int kStartY = 50;      // Y-координата первой строки
    static constexpr int kLineHeight = 25;  // Высота одной строки
};

}  // namespace client

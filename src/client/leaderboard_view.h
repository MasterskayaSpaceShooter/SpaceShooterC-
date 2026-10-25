#pragma once
#include "client_world.h"
#include "renderer.h"

namespace client {
class LeaderboardView {
public:
    void render(IRenderer& renderer, const ClientWorld& world);
};
}  // namespace client

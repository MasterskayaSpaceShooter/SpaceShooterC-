#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>
#include <SFML/Graphics.hpp>

#include "client_state.h"
#include "client_world.h"
#include "death_screen.h"
#include "input_provider.h"
#include "leaderboard_view.h"
#include "logger.h"
#include "protocol.h"
#include "renderer.h"
#include "score_view.h"
#include "tcp_client.h"

namespace client {

class GameController {
public:
    GameController(sf::RenderWindow& window,
                   std::shared_ptr<TcpClient> tcp,
                   std::shared_ptr<IRenderer> renderer,
                   std::shared_ptr<IInputProvider> input_provider,
                   std::string player_name);

    ~GameController() = default;

    GameController(const GameController&) = delete;
    GameController& operator=(const GameController&) = delete;
    GameController(GameController&&) = delete;
    GameController& operator=(GameController&&) = delete;

    void run();
    void update(double dt);
    void render();

    [[nodiscard]] ClientState getState() const noexcept { return state_; }
    void setState(ClientState new_state) noexcept;
    [[nodiscard]] types::EntityId getMyPlayerId() const noexcept { return my_player_id_; }
    [[nodiscard]] const ClientWorld& getWorld() const noexcept { return world_; }
    [[nodiscard]] const std::string& getPlayerName() const noexcept { return player_name_; }

    void handleIncomingBytes(const std::vector<uint8_t>& data);

private:
    void sendString(std::string_view str);

private:
    sf::RenderWindow& window_;
    std::shared_ptr<TcpClient> tcp_;
    std::shared_ptr<IRenderer> renderer_;
    std::shared_ptr<IInputProvider> input_provider_;
    std::string player_name_;

    ClientState state_{ClientState::Connecting};
    types::EntityId my_player_id_{0};
    bool has_received_first_snapshot_{false};

    mutable std::mutex world_mutex_;
    ClientWorld world_;

    LeaderboardView leaderboard_view_;
    ScoreView score_view_;
    DeathScreen death_screen_;

    double shoot_cooldown_{0.0};
};

}  // namespace client

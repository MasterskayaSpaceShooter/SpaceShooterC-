#include "game_controller.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>

namespace client {

namespace {
constexpr double kShootCooldownDuration = 0.25;
}  // namespace

GameController::GameController(sf::RenderWindow& window,
                               std::shared_ptr<TcpClient> tcp,
                               std::shared_ptr<IRenderer> renderer,
                               std::shared_ptr<IInputProvider> input_provider,
                               std::string player_name)
    : window_(window),
      tcp_(std::move(tcp)),
      renderer_(std::move(renderer)),
      input_provider_(std::move(input_provider)),
      player_name_(std::move(player_name)) {
    if (!tcp_) {
        LOG_ERROR("[GameController] tcp client pointer is nullptr");
        throw std::invalid_argument("GameController: tcp must not be nullptr");
    }
    if (!renderer_) {
        LOG_ERROR("[GameController] renderer pointer is nullptr");
        throw std::invalid_argument("GameController: renderer must not be nullptr");
    }
    if (!input_provider_) {
        LOG_ERROR("[GameController] input_provider pointer is nullptr");
        throw std::invalid_argument("GameController: input_provider must not be nullptr");
    }
}

void GameController::setState(ClientState new_state) noexcept {
    if (state_ != new_state) {
        LOG_INFO("[GameController] State changed: {} -> {}",
                 static_cast<int>(state_),
                 static_cast<int>(new_state));
        state_ = new_state;
    }
}

void GameController::sendString(std::string_view str) {
    std::vector<uint8_t> data(str.begin(), str.end());
    tcp_->send(std::move(data));
}

void GameController::run() {
    LOG_INFO("[GameController] Initializing network callbacks for player '{}'", player_name_);

    tcp_->setMessageCallback([this](const std::vector<uint8_t>& data) {
        handleIncomingBytes(data);
    });

    LOG_INFO("[GameController] Sending JOIN command for player '{}'", player_name_);
    sendString(std::format("JOIN {}\n", player_name_));
    setState(ClientState::Joining);

    using Clock = std::chrono::steady_clock;
    auto last_time = Clock::now();

    while (window_.isOpen() && state_ != ClientState::Disconnected) {
        // Обязательная откачка сообщений окна ОС для предотвращения зависания
        sf::Event event;
        while (window_.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window_.close();
                setState(ClientState::Disconnected);
                break;
            }
        }

        auto now = Clock::now();
        double dt = std::chrono::duration<double>(now - last_time).count();
        last_time = now;

        update(dt);
        render();

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    LOG_INFO("[GameController] Main game loop finished, client stopped");
}

void GameController::update(double dt) {
    if (state_ == ClientState::Connecting || state_ == ClientState::Disconnected) {
        return;
    }

    math::Vector2D my_pos(0.0, 0.0);

    {
        std::lock_guard lock(world_mutex_);
        const snapshot::EntitySnapshot* my_ship = nullptr;
        for (const auto& [id, entity] : world_.getEntities()) {
            if (id == my_player_id_) {
                my_ship = &entity;
                break;
            }
        }

        // Проверяем смерть корабля только после того, как получили хотя бы один снапшот мира
        if (state_ == ClientState::InGame && has_received_first_snapshot_) {
            if (!my_ship || my_ship->is_destroyed) {
                LOG_INFO("[GameController] Player ship destroyed, entering GameOver state");
                setState(ClientState::GameOver);
            }
        }

        if (my_ship && my_ship->transform.has_value()) {
            my_pos = my_ship->transform->position;
        }
    }

    auto input = input_provider_->pollInput(my_pos);

    if (state_ == ClientState::GameOver) {
        if (input.request_respawn) {
            LOG_INFO("[GameController] Player requested RESPAWN, sending command to server");
            sendString("RESPAWN\n");
            has_received_first_snapshot_ = false;
            setState(ClientState::Joining);
        }
        return;
    }

    if (state_ != ClientState::InGame) {
        return;
    }

    if (input.move_direction.lengthSquared() > 0.0) {
            LOG_INFO("[GameController] Sending MOVE angle={}", input.target_angle);
            sendString(std::format("MOVE {:.4f}\n", input.target_angle));
        }

        shoot_cooldown_ = std::max(0.0, shoot_cooldown_ - dt);
        if (input.is_shooting && shoot_cooldown_ <= 0.0) {
            shoot_cooldown_ = kShootCooldownDuration;
            LOG_INFO("[GameController] Sending SHOOT");
            sendString("SHOOT\n");
        }
}

void GameController::render() {
    if (state_ == ClientState::Joining) {
        renderer_->renderWelcomeWindow(player_name_);
    } else if (state_ == ClientState::InGame) {
        std::lock_guard lock(world_mutex_);
        renderer_->renderGame(world_);
    } else if (state_ == ClientState::GameOver) {
        renderer_->renderDeathScreen(0);
    }
}

void GameController::handleIncomingBytes(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return;
    }

    std::string_view sv(reinterpret_cast<const char*>(data.data()), data.size());
    if (sv.starts_with("ACCEPT ")) {
        auto num_str = sv.substr(7);
        while (!num_str.empty() && (num_str.back() == '\r' || num_str.back() == '\n' || num_str.back() == ' ')) {
            num_str.remove_suffix(1);
        }
        try {
            const auto raw_id = static_cast<types::EntityId::value_type>(std::stoull(std::string(num_str)));
            my_player_id_ = types::EntityId(raw_id);
            LOG_INFO("[GameController] ACCEPT received: assigned EntityId={}", my_player_id_.get());
            setState(ClientState::InGame);
        } catch (const std::exception& e) {
            LOG_ERROR("[GameController] Failed to parse ID from ACCEPT '{}': {}", sv, e.what());
        }
        return;
    }

    // Если это не текстовый ACCEPT, это бинарный GameSnapshot:
    try {
            auto game_snapshot = snapshot::GameSnapshot::Deserialize(data);

            std::lock_guard lock(world_mutex_);
            world_.updateSnapshot(game_snapshot, my_player_id_);

                    LOG_DEBUG("[GameController] Snapshot received, payload_size={}, entities={}",
                              data.size(),
                              world_.getEntities().size());

            if (!has_received_first_snapshot_) {
                has_received_first_snapshot_ = true;
                LOG_INFO("[GameController] First snapshot registered! Entities in world: {}", world_.getEntities().size());
            }
        } catch (const std::exception& e) {
            LOG_WARN("[GameController] Failed to deserialize GameSnapshot: {}", e.what());
        }
}

}  // namespace client

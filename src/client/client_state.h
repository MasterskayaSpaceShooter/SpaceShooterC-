#pragma once

#include <cstdint>
#include <string_view>

namespace client {

enum class ClientState : std::uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Joining,
    InGame,
    /// Match ended for this client (server sent GAME_END).
    /// NOT terminal for the client.
    GameOver,
};

/// Human-readable name of a state, for logs.
[[nodiscard]] inline constexpr std::string_view convertStateToString(ClientState s) noexcept {
    switch (s) {
        case ClientState::Disconnected:
            return "Disconnected";
        case ClientState::Connecting:
            return "Connecting";
        case ClientState::Connected:
            return "Connected";
        case ClientState::Joining:
            return "Joining";
        case ClientState::InGame:
            return "InGame";
        case ClientState::GameOver:
            return "GameOver";
    }
    return "Unknown";
}

}  // namespace client

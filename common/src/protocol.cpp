#include "common/protocol.hpp"

namespace common {
std::string getCommandName(GameCommand cmd) {
    switch (cmd) {
        case GameCommand::JoinGame:
            return "JOIN_GAME";
        case GameCommand::Move:
            return "MOVE";
        case GameCommand::Shoot:
            return "SHOOT";
        case GameCommand::LeaveGame:
            return "LEAVE_GAME";
        default:
            return "UNKNOWN";
    }
}
}  // namespace common

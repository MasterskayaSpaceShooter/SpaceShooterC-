#include "protocol.h"

#include <string>
#include <string_view>
#include <vector>

namespace protocol {
namespace {

std::vector<std::uint8_t> toBytes(std::string_view s) {
    return std::vector<std::uint8_t>(s.begin(), s.end());
}

std::string_view toWire(types::Direction d) {
    switch (d) {
        case types::Direction::Up:
            return "up";
        case types::Direction::Down:
            return "down";
        case types::Direction::Left:
            return "left";
        case types::Direction::Right:
            return "right";
    }
    return {};
}

}  // namespace

// ---- Client -> Server ----

std::vector<std::uint8_t> encode(const JoinMsg& m) {
    std::string line = "JOIN ";
    line.append(m.playerName);
    return toBytes(line);
}

std::vector<std::uint8_t> encode(const MoveMsg& m) {
    std::string line = "MOVE ";
    line.append(toWire(m.direction));
    return toBytes(line);
}

std::vector<std::uint8_t> encodeAttack() {
    return toBytes("ATTACK");
}

// ---- Server -> Client ----

ServerMsg decode(std::span<const std::uint8_t> wire) {
    const std::string_view text(reinterpret_cast<const char*>(wire.data()), wire.size());

    const auto sp = text.find(' ');
    const std::string_view keyword = (sp == std::string_view::npos) ? text : text.substr(0, sp);
    const std::string_view argView = (sp == std::string_view::npos) ? std::string_view{} : text.substr(sp + 1);

    ServerMsgType type = ServerMsgType::Unknown;
    if (keyword == "ACCEPT")
        type = ServerMsgType::Accept;
    else if (keyword == "REJECT")
        type = ServerMsgType::Reject;
    else if (keyword == "GAME_END")
        type = ServerMsgType::GameEnd;

    return {type, std::string(argView)};  // own the data — no dangling views
}

}  // namespace protocol

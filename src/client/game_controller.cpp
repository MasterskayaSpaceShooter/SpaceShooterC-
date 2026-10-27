#include "game_controller.h"

#include <boost/asio/post.hpp>
#include <stdexcept>
#include <string_view>

#include "logger.h"
#include "protocol.h"

namespace client {

std::shared_ptr<GameController> GameController::create(TcpClientPtr tcp, boost::asio::io_context& io) {
    if (!tcp) {
        throw std::invalid_argument("GameController::create: tcp must not be null");
    }

    struct MakeSharedEnabler final : GameController {
        MakeSharedEnabler(TcpClientPtr t, boost::asio::io_context& i) : GameController(std::move(t), i) {}
    };

    std::shared_ptr<GameController> self = std::make_shared<MakeSharedEnabler>(std::move(tcp), io);
    self->wireCallbacks();
    LOG_INFO("GameController created");
    return self;
}

void GameController::start(std::string host, std::uint16_t port, std::string playerName) {
    if (!validatePlayerName(playerName)) {
        return;
    }
    LOG_INFO("start() requested: host='{}' port={} player='{}'", host, port, playerName);
    boost::asio::post(
        strand_,
        [self = shared_from_this(), host = std::move(host), port, playerName = std::move(playerName)]() mutable {
            self->doStart(std::move(host), port, std::move(playerName));
        });
}

void GameController::shutdown() {
    LOG_INFO("shutdown() requested (current state: {})", convertStateToString(state_.load()));
    boost::asio::post(strand_, [self = shared_from_this()] {
        self->doShutdown();
    });
}

void GameController::rejoin(std::string host, std::uint16_t port, std::string playerName) {
    if (!validatePlayerName(playerName)) {
        return;
    }
    LOG_INFO("rejoin() requested: host='{}' port={} player='{}'", host, port, playerName);
    boost::asio::post(
        strand_,
        [self = shared_from_this(), host = std::move(host), port, playerName = std::move(playerName)]() mutable {
            // doShutdown() is unconditional; it tears down the current
            // socket (if any) and transitions to Disconnected, so doStart()
            // is guaranteed to pass its own guard below.
            self->doShutdown();
            self->doStart(std::move(host), port, std::move(playerName));
        });
}

ClientState GameController::state() const noexcept {
    return state_.load();
}

void GameController::wireCallbacks() {
    auto weak = weak_from_this();

    tcp_->setMessageCallback([weak](std::vector<std::uint8_t> payload) {
        if (auto self = weak.lock())
            boost::asio::post(self->strand_, [self, payload = std::move(payload)] {
                self->onMessageOnStrand(payload);
            });
    });

    tcp_->setDisconnectCallback([weak] {
        if (auto self = weak.lock())
            boost::asio::post(self->strand_, [self] {
                const auto s = self->state_.load();
                // Stale event from a superseded connection (e.g. the disconnect
                // posted by rejoin()'s doShutdown() firing after doStart()
                // already started a new connect), or a redundant event while
                // already torn down. Drop.
                if (s == ClientState::Disconnected || s == ClientState::Connecting) {
                    LOG_INFO("Ignoring disconnect event (state={})", convertStateToString(s));
                    return;
                }
                LOG_INFO("TcpClient reported disconnect");
                self->setState(ClientState::Disconnected);
            });
    });
}

void GameController::doStart(const std::string& host, std::uint16_t port, std::string playerName) {
    if (state_.load() != ClientState::Disconnected) {
        LOG_WARN("doStart() ignored: state={} (expected Disconnected)", convertStateToString(state_.load()));
        return;
    }

    playerName_ = std::move(playerName);
    setState(ClientState::Connecting);
    LOG_INFO("Connecting to {}:{} as '{}'", host, port, playerName_);

    const std::uint64_t gen = ++connectGen_;
    auto weak = weak_from_this();
    tcp_->connect(host, port, [weak, gen](bool ok) {
        if (auto self = weak.lock())
            boost::asio::post(self->strand_, [self, ok, gen] {
                // Generation check first: a callback belonging to a superseded
                // connect (rejoin / shutdown+start) must be ignored even if
                // state_ happens to read Connecting again.
                if (self->connectGen_ != gen) {
                    LOG_INFO("Ignoring stale connect callback (gen {} vs current {})", gen, self->connectGen_);
                    return;
                }
                if (self->state_.load() != ClientState::Connecting) {
                    LOG_WARN("Connect callback arrived but state={} (expected Connecting)",
                             convertStateToString(self->state_.load()));
                    return;
                }

                if (!ok) {
                    LOG_WARN("TCP connect failed");
                    self->setState(ClientState::Disconnected);
                    return;
                }
                LOG_INFO("TCP connection established");
                self->setState(ClientState::Connected);
                self->sendJoin();
            });
    });
}

void GameController::doShutdown() {
    LOG_INFO("Shutting down transport (state: {})", convertStateToString(state_.load()));
    ++connectGen_;  // invalidate any in-flight connect callback
    tcp_->disconnect();
    setState(ClientState::Disconnected);
    // Callbacks stay wired; the Disconnected guard in onMessageOnStrand()
    // drops late messages, so rejoin()/start() reuse the same wiring.
}

void GameController::sendJoin() {
    LOG_INFO("Sending JOIN as '{}'", playerName_);
    tcp_->send(protocol::encode(protocol::JoinMsg{playerName_}));
    setState(ClientState::Joining);
}

void GameController::onMessageOnStrand(const std::vector<std::uint8_t>& payload) {
    if (state_.load() == ClientState::Disconnected) {
        LOG_WARN("Dropping server message while Disconnected ({} bytes)", payload.size());
        return;
    }

    const protocol::ServerMsg msg = protocol::decode(payload);

    switch (msg.type) {
        case protocol::ServerMsgType::Accept:
            LOG_INFO("ACCEPT (argument='{}')", msg.argument);
            setState(ClientState::InGame);
            break;

        case protocol::ServerMsgType::Reject:
            LOG_WARN("REJECT (argument='{}')", msg.argument);
            // Server refused this session; treat as terminal for the
            // current attempt. App may still rejoin() later.
            doShutdown();
            break;

        case protocol::ServerMsgType::GameEnd:
            LOG_INFO("GAME_END (argument='{}')", msg.argument);
            // Design A: match over for this client. Socket is left open —
            // the server may send a final score, or close it. Either way
            // the app drives rejoin() from its own countdown.
            setState(ClientState::GameOver);
            break;

        case protocol::ServerMsgType::Unknown: {
            if (payload.empty()) {
                LOG_WARN("Empty server message");
            } else {
                const std::string_view raw(reinterpret_cast<const char*>(payload.data()), payload.size());
                LOG_WARN("Unknown server message ({} bytes): '{}'", payload.size(), raw);
            }
            break;
        }
    }
}

void GameController::setState(ClientState next) noexcept {
    const auto current = state_.load();
    if (current == next)
        return;
    state_.store(next);
    LOG_INFO("GameController state: {} -> {}", convertStateToString(current), convertStateToString(next));
}

bool GameController::validatePlayerName(const std::string& name) {
    if (name.empty()) {
        LOG_ERROR("playerName must not be empty");
        return false;
    }
    for (unsigned char c : name) {
        if (c <= 0x20 || c == 0x7f) {
            LOG_ERROR("playerName must not contain whitespace or control characters");
            return false;
        }
    }
    return true;
}

}  // namespace client

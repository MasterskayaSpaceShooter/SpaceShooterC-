#include "game_controller.h"

#include <boost/asio/post.hpp>
#include <stdexcept>
#include <string_view>

#include "logger.h"

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
    LOG_INFO("start() requested: host='{}' port={} player='{}'", host, port, playerName);

    // No state check here — it would race with a concurrent start() or
    // shutdown(). Post unconditionally; doStart() decides on the strand.
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

    auto weak = weak_from_this();
    tcp_->connect(host, port, [weak](bool ok) {
        if (auto self = weak.lock())
            boost::asio::post(self->strand_, [self, ok] {
                // If shutdown() ran while the connect was in flight, we are
                // no longer Connecting. Bail — do not touch tcp_ further.
                if (self->state_.load() != ClientState::Connecting) {
                    LOG_WARN(
                        "Connect callback arrived but state={} (expected Connecting) — "
                        "shutdown() likely raced with connect",
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
    tcp_->disconnect();
    setState(ClientState::Disconnected);
}

void GameController::sendJoin() {
    std::string wire = "JOIN " + playerName_;
    LOG_INFO("Sending JOIN as '{}'", playerName_);
    tcp_->send(std::vector<std::uint8_t>(wire.begin(), wire.end()));
    setState(ClientState::Joining);
}

void GameController::onMessageOnStrand(const std::vector<std::uint8_t>& payload) {
    if (payload.empty()) {
        LOG_WARN("Received empty message from server");
        return;
    }

    const std::string_view msg(reinterpret_cast<const char*>(payload.data()), payload.size());

    if (msg.starts_with("ACCEPT")) {
        LOG_INFO("JOIN accepted by server");
        setState(ClientState::InGame);
    } else if (msg.starts_with("REJECT")) {
        LOG_WARN("JOIN rejected by server");
        // mvp REJECT = disconnect, so TcpClient & GameController
        // states synchronized (both Disconnected)
        tcp_->disconnect();
        setState(ClientState::Disconnected);
    } else {
        LOG_WARN("Unknown server message ({} bytes): '{}'", payload.size(), msg);
    }
}

void GameController::setState(ClientState next) noexcept {
    const auto current = state_.load();
    if (current == next)
        return;
    state_.store(next);
    LOG_INFO("GameController state: {} -> {}", convertStateToString(current), convertStateToString(next));
}

}  // namespace client

#include "network_response_router.h"

#include <string_view>
#include <utility>
#include <vector>

#include "logger.h"
#include "network_events.h"

std::shared_ptr<NetworkResponseRouter> NetworkResponseRouter::create(
    std::shared_ptr<events::EventBus> event_bus) {
    auto router = std::make_shared<NetworkResponseRouter>(std::move(event_bus));
    router->setupSubscriptions();
    return router;
}

NetworkResponseRouter::NetworkResponseRouter(std::shared_ptr<events::EventBus> event_bus)
    : event_bus_(std::move(event_bus)) {}

void NetworkResponseRouter::start() {
    LOG_INFO("[NetworkResponseRouter] Started");
}

void NetworkResponseRouter::setMessageHandler(MessageHandler handler) {
    message_handler_ = std::move(handler);
}

void NetworkResponseRouter::onMessageReceived(const NetworkMessageEvent& event) {
    if (message_handler_) {
        message_handler_(event.session_id, event.payload);
    }

    std::string_view sv(reinterpret_cast<const char*>(event.payload.data()), event.payload.size());

    while (!sv.empty() && (sv.back() == '\r' || sv.back() == '\n')) {
        sv.remove_suffix(1);
    }

    if (sv.starts_with("JOIN")) {
        std::string_view login = sv.substr(4);
        while (!login.empty() && login.front() == ' ') {
            login.remove_prefix(1);
        }

        if (!login.empty()) {
            event_bus_->publish(PlayerJoinEvent{event.session_id, std::string(login)});
            LOG_INFO("Session {}: Published PlayerJoinEvent with login '{}'", event.session_id, login);
        }
    } else if (sv.starts_with("MOVE")) {
        std::string_view angle_str = sv.substr(4);
        while (!angle_str.empty() && angle_str.front() == ' ') {
            angle_str.remove_prefix(1);
        }

        try {
            double angle = std::stod(std::string(angle_str));
            event_bus_->publish(PlayerMoveEvent{event.session_id, angle});
        } catch (...) {
            LOG_WARN("Session {}: Failed to parse angle: '{}'", event.session_id, angle_str);
        }
    } else if (sv == "SHOOT") {
        event_bus_->publish(PlayerShootEvent{event.session_id});
        event_bus_->publish(ShotMessageEvent{event.session_id});
        LOG_DEBUG("Session {}: Published PlayerShootEvent", event.session_id);
    } else if (sv.starts_with("RESPAWN")) {
        event_bus_->publish(PlayerRespawnEvent{event.session_id});
        LOG_INFO("Session {}: Published PlayerRespawnEvent", event.session_id);
    }
}

void NetworkResponseRouter::onClientConnected(const ClientConnectedEvent& event) {
    LOG_INFO("Session {}: Client connected from {}", event.session_id, event.remote_address);
}

void NetworkResponseRouter::onClientDisconnected(const ClientDisconnectedEvent& event) {
    LOG_INFO("Session {}: Client disconnected", event.session_id);
}

void NetworkResponseRouter::setupSubscriptions() {
    subscriptions_.emplace_back(
        event_bus_->subscribe<NetworkMessageEvent>([this](const NetworkMessageEvent& event) {
            onMessageReceived(event);
        }));

    subscriptions_.emplace_back(
        event_bus_->subscribe<ClientConnectedEvent>([this](const ClientConnectedEvent& event) {
            onClientConnected(event);
        }));

    subscriptions_.emplace_back(
        event_bus_->subscribe<ClientDisconnectedEvent>([this](const ClientDisconnectedEvent& event) {
            onClientDisconnected(event);
        }));
}

void NetworkResponseRouter::sendTo(SessionId session_id, std::vector<uint8_t> payload) {
    event_bus_->publish(SendPacketEvent{session_id, std::move(payload)});
}

void NetworkResponseRouter::broadcast(std::vector<uint8_t> payload) {
    event_bus_->publish(SendPacketEvent{kBroadcastSessionId, std::move(payload)});
}

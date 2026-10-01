#include <utility>

#include "network_response_router.h"

NetworkResponseRouter::NetworkResponseRouter(events::EventBus& event_bus) : event_bus_(event_bus) {
    setupSubscriptions();
}

NetworkResponseRouter::~NetworkResponseRouter() {
    // Отключаем все подписки на события шины
    for (auto& connection : subscriptions_) {
        connection.disconnect();
    }
}

void NetworkResponseRouter::setMessageHandler(MessageHandler handler) {
    message_handler_ = std::move(handler);
}

void NetworkResponseRouter::onMessageReceived(const NetworkMessageEvent& event) {
    if (message_handler_) {
        message_handler_(event.session_id, event.payload);
    }
}

void NetworkResponseRouter::setupSubscriptions() {
    subscriptions_.emplace_back(event_bus_.subscribe<NetworkMessageEvent>([this](const NetworkMessageEvent& event) {
        onMessageReceived(event);
    }));

    subscriptions_.emplace_back(event_bus_.subscribe<ClientConnectedEvent>([this](const ClientConnectedEvent& event) {
        onClientConnected(event);
    }));

    subscriptions_.emplace_back(
        event_bus_.subscribe<ClientDisconnectedEvent>([this](const ClientDisconnectedEvent& event) {
            onClientDisconnected(event);
        }));
}

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

void NetworkResponseRouter::sendTo(SessionId session_id, std::vector<uint8_t> payload) {
    // Формируем и публикуем событие отправки пакета целевому клиенту
    event_bus_.publish(SendPacketEvent{session_id, std::move(payload)});
}

void NetworkResponseRouter::broadcast(std::vector<uint8_t> payload) {
    // Формируем и публикуем событие массовой рассылки пакета всем клиентам.
    // session_id = 0 используется как маркер Broadcast.
    event_bus_.publish(SendPacketEvent{0, std::move(payload)});
}

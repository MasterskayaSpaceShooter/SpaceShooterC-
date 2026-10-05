#include "network_response_router.h"

std::shared_ptr<NetworkResponseRouter> NetworkResponseRouter::create(std::shared_ptr<events::EventBus> event_bus) {
    if (!event_bus) {
        LOG_ERROR("NetworkResponseRouter::create: event_bus не может быть nullptr");
        return nullptr;
    }
    std::shared_ptr<NetworkResponseRouter> router(new NetworkResponseRouter(std::move(event_bus)));
    // Подписки устанавливаем после передачи объекта во владение shared_ptr:
    // только тогда weak_from_this() в слотах будет корректно захватывать себя.
    router->setupSubscriptions();
    return router;
}

NetworkResponseRouter::NetworkResponseRouter(std::shared_ptr<events::EventBus> event_bus) :
    event_bus_(std::move(event_bus)) {}

NetworkResponseRouter::~NetworkResponseRouter() {
    // scoped_connection автоматически отключает подписки; очищаем вектор явно
    subscriptions_.clear();
}

void NetworkResponseRouter::setMessageHandler(MessageHandler handler) {
    std::unique_lock<std::shared_mutex> lock(message_handler_mutex_);
    message_handler_ = std::move(handler);
}

void NetworkResponseRouter::onMessageReceived(const NetworkMessageEvent& event) {
    MessageHandler handler;
    {
        std::shared_lock<std::shared_mutex> lock(message_handler_mutex_);
        handler = message_handler_;
    }
    if (handler) {
        handler(event.session_id, event.payload);
    }
}

void NetworkResponseRouter::onClientConnected(const ClientConnectedEvent& event) {
    // TODO: Убрать после подключения логгера
    // Заглушка LOG_INFO ((void)0) отбрасывает аргумент макроса,
    // поэтому параметр помечаем использованным явно.
    (void)event;
    LOG_INFO(std::format("Клиент подключен: session_id={}, адрес={}", event.session_id, event.remote_address));
}

void NetworkResponseRouter::onClientDisconnected(const ClientDisconnectedEvent& event) {
    (void)event;
    LOG_INFO(std::format("Клиент отключен: session_id={}", event.session_id));
}

void NetworkResponseRouter::setupSubscriptions() {
    // Захватываем слабый указатель на себя, чтобы не держать объект живым в слотах шины
    // и избежать вызова на освобождённом объекте при конкурентной публикации см. примечание
    // к NetworkResponseRouter::create в network_response_router.h.
    auto self = weak_from_this();

    subscriptions_.emplace_back(event_bus_->subscribe<NetworkMessageEvent>([self](const NetworkMessageEvent& event) {
        if (auto s = self.lock()) {
            s->onMessageReceived(event);
        }
    }));

    subscriptions_.emplace_back(event_bus_->subscribe<ClientConnectedEvent>([self](const ClientConnectedEvent& event) {
        if (auto s = self.lock()) {
            s->onClientConnected(event);
        }
    }));

    subscriptions_.emplace_back(
        event_bus_->subscribe<ClientDisconnectedEvent>([self](const ClientDisconnectedEvent& event) {
            if (auto s = self.lock()) {
                s->onClientDisconnected(event);
            }
        }));
}

void NetworkResponseRouter::sendTo(SessionId session_id, std::vector<uint8_t> payload) {
    // Формируем и публикуем событие отправки пакета целевому клиенту
    event_bus_->publish(SendPacketEvent{session_id, std::move(payload)});
}

void NetworkResponseRouter::broadcast(std::vector<uint8_t> payload) {
    // Формируем и публикуем событие массовой рассылки пакета всем клиентам.
    // BROADCAST_SESSION_ID используется как маркер Broadcast.
    event_bus_->publish(SendPacketEvent{BROADCAST_SESSION_ID, std::move(payload)});
}

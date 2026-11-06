#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <boost/signals2.hpp>

#include "event_bus.h"
#include "network_events.h"

class NetworkResponseRouter : public std::enable_shared_from_this<NetworkResponseRouter> {
public:
    using MessageHandler = std::function<void(SessionId, std::vector<uint8_t>)>;
    inline static constexpr SessionId kBroadcastSessionId = 0;

    static std::shared_ptr<NetworkResponseRouter> create(std::shared_ptr<events::EventBus> event_bus);

    explicit NetworkResponseRouter(std::shared_ptr<events::EventBus> event_bus);
    virtual ~NetworkResponseRouter() = default;

    void start();
    void setMessageHandler(MessageHandler handler);

    void onMessageReceived(const NetworkMessageEvent& event);
    void onClientConnected(const ClientConnectedEvent& event);
    void onClientDisconnected(const ClientDisconnectedEvent& event);

    void sendTo(SessionId session_id, std::vector<uint8_t> payload);
    void broadcast(std::vector<uint8_t> payload);

private:
    void setupSubscriptions();

    std::shared_ptr<events::EventBus> event_bus_;
    MessageHandler message_handler_;
    std::vector<boost::signals2::scoped_connection> subscriptions_;
};

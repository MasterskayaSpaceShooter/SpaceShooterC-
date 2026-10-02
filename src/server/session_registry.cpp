#include "session_registry.h"

#include <memory>
#include <mutex>
#include <vector>

#include "network_events.h"
#include "session.h"

SessionRegistry::SessionRegistry(events::EventBus& event_bus) : event_bus_(event_bus) {
    event_bus_.subscribe<SendPacketEvent>([this](const SendPacketEvent& event) {
        if (this->sessions_.count(event.session_id) > 0) {
            auto session = getSession(event.session_id);
            session->send(event.payload);
        }
    });
}

void SessionRegistry::addSession(std::shared_ptr<Session> session) {
    std::lock_guard lock{registry_mutex_};
    SessionId id = session->getId();
    sessions_.insert({id, session});
}

void SessionRegistry::removeSession(SessionId id) {
    std::lock_guard lock{registry_mutex_};
    sessions_.erase(id);
}

void SessionRegistry::broadcast(const std::vector<uint8_t>& data) {
    std::vector<std::shared_ptr<Session>> sessions;
    {
        std::lock_guard lock{registry_mutex_};
        for (auto& [_, session] : sessions_) {
            sessions.push_back(session);
        }
    }
    for (auto& session : sessions) {
        session->send(data);
    }
}

std::shared_ptr<Session> SessionRegistry::getSession(SessionId id) {
    {
        std::lock_guard lock{registry_mutex_};
        if (sessions_.count(id) > 0) {
            return sessions_.at(id);
        }
    }
    return nullptr;
}

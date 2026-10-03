#include "session_registry.h"

#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "network_events.h"
#include "session.h"

SessionRegistry::SessionRegistry(std::shared_ptr<events::EventBus> event_bus) : event_bus_(event_bus) {
    if (event_bus_ == nullptr) {
        throw std::invalid_argument("Eventbus is null");
    }
    subscription_ = event_bus_->subscribe<SendPacketEvent>([this](const SendPacketEvent& event) {
        std::shared_ptr<Session> session;
        {
            std::lock_guard lock{registry_mutex_};
            auto it = sessions_.find(event.session_id);
            if (it != sessions_.end()) {
                session = it->second;
            }
        }
        if (session != nullptr) {
            session->send(event.payload);
        }
    });
    if (!subscription_.connected()) {
        throw std::runtime_error("Error event subsription");
    }
}

void SessionRegistry::addSession(std::shared_ptr<Session> session) {
    if (session == nullptr) {
        return;
    }
    std::lock_guard lock{registry_mutex_};
    SessionId id = session->getId();
    // if the sessions_ by this id already contains then return
    if (sessions_.count(id) > 0) {
        return;
    }
    sessions_.insert({id, session});
}

size_t SessionRegistry::removeSession(SessionId id) {
    std::lock_guard lock{registry_mutex_};
    return sessions_.erase(id);
}

void SessionRegistry::broadcast(const std::vector<uint8_t>& data) {
    std::vector<std::shared_ptr<Session>> sessions;
    sessions.reserve(sessions_.size());
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
        if (auto it = sessions_.find(id); it != sessions_.end()) {
            return it->second;
        }
    }
    return nullptr;
}

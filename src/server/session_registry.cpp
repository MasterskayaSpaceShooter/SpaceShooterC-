#include "session_registry.h"

#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "event_bus.h"
#include "network_events.h"
#include "session.h"

// remove after valid logger implementation
#ifndef LOG_INFO
#define LOG_INFO(msg) (void)0
#endif

#ifndef LOG_ERROR
#define LOG_ERROR(msg) (void)0
#endif

SessionRegistry::SessionRegistry(std::shared_ptr<events::EventBus> event_bus) : event_bus_(event_bus) {
    if (event_bus_ == nullptr) {
        throw std::invalid_argument("Eventbus is null");
    }

    send_subscription_ = event_bus_->subscribe<SendPacketEvent>([this](const SendPacketEvent& event) {
        std::shared_ptr<Session> session;
        {
            std::lock_guard lock{registry_mutex_};
            auto it = sessions_.find(event.session_id);
            if (it != sessions_.end()) {
                session = it->second;
            }
        }
        if (session != nullptr) {
            if (event.session_id == 0) {
                this->broadcast(event.payload);
            } else {
                session->send(event.payload);
            }
        }
    });

    remove_subscription_ = event_bus_->subscribe<ClientDisconnectedEvent>([this](const ClientDisconnectedEvent& event) {
        this->removeSession(event.session_id);
    });

    if (!send_subscription_.connected() || !remove_subscription_.connected()) {
        throw std::runtime_error("Error event subsription");
    }
    LOG_INFO("SessionRegistry created succesfully");
}

void SessionRegistry::addSession(std::shared_ptr<Session> session) {
    if (session == nullptr) {
        LOG_ERROR("session was not added, session is nullptr");
        return;
    }
    std::lock_guard lock{registry_mutex_};
    SessionId id = session->getId();
    // if the sessions_ by this id already contains then return
    if (sessions_.emplace(id, session).second) {
        LOG_INFO("Session added succesfully");
        return;
    }
    LOG_ERROR("session was not added, this id is used");
}

void SessionRegistry::removeSession(SessionId id) {
    std::shared_ptr<Session> session_to_remove;
    {
        std::lock_guard lock{registry_mutex_};
        auto it = sessions_.find(id);
        if (it == sessions_.end()) {
            LOG_ERROR("session was not removed, session is nullptr");
            return;
        }
        session_to_remove = it->second;
        sessions_.erase(id);
    }
    session_to_remove->close();
    LOG_INFO("session removed succesfully");
}

void SessionRegistry::broadcast(const std::vector<uint8_t>& data) {
    std::vector<std::shared_ptr<Session>> sessions;
    {
        std::lock_guard lock{registry_mutex_};
        sessions.reserve(sessions_.size());
        for (auto& [_, session] : sessions_) {
            sessions.push_back(session);
        }
    }
    for (auto& session : sessions) {
        session->send(data);
    }
    LOG_INFO("Data sended");
}

std::shared_ptr<Session> SessionRegistry::getSession(SessionId id) {
    {
        std::lock_guard lock{registry_mutex_};
        if (auto it = sessions_.find(id); it != sessions_.end()) {
            LOG_INFO("session returned");
            return it->second;
        }
    }
    LOG_ERROR("session is null by this id");
    return nullptr;
}

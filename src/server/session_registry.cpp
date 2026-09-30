#include "session_registry.h"

#include <stdexcept>

#include "session.h"

SessionRegistry::SessionRegistry(events::EventBus& eventbus) : event_bus_(eventbus) {}

void addSession(std::shared_ptr<Session> session) {
    return;
}

void SessionRegistry::removeSession(SessionId id) {
    return;
}

std::shared_ptr<Session> SessionRegistry::getSession(SessionId id) {
    throw std::runtime_error("Empty method");
}

void SessionRegistry::broadcast(const std::vector<uint8_t>& data) {
    return;
}

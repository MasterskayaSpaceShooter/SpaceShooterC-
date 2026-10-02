#include "session_registry.h"

#include <stdexcept>

SessionRegistry::SessionRegistry(events::EventBus& event_bus) : event_bus_(event_bus) {}

void SessionRegistry::addSession(std::shared_ptr<Session> session) {
    return;
}

void SessionRegistry::removeSession(SessionId id) {
    return;
}

void SessionRegistry::broadcast(const std::vector<uint8_t>& data) {
    return;
}

std::shared_ptr<Session> SessionRegistry::getSession(SessionId id) {
    throw std::runtime_error("Empty body of method");
}

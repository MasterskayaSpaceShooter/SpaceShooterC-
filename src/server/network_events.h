#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "event_bus.h"

using SessionId = std::uint32_t;

struct NetworkEvent : public events::Event {
    SessionId session_id{0};

    NetworkEvent() = default;
    explicit NetworkEvent(SessionId id) : session_id(id) {}
    ~NetworkEvent() override = default;
};

struct ClientConnectedEvent : public NetworkEvent {
    std::string remote_address;

    ClientConnectedEvent() = default;
    ClientConnectedEvent(SessionId id, std::string addr)
        : NetworkEvent(id), remote_address(std::move(addr)) {}
};

struct ClientDisconnectedEvent : public NetworkEvent {
    ClientDisconnectedEvent() = default;
    explicit ClientDisconnectedEvent(SessionId id) : NetworkEvent(id) {}
};

struct NetworkMessageEvent : public NetworkEvent {
    std::vector<uint8_t> payload;

    NetworkMessageEvent() = default;
    NetworkMessageEvent(SessionId id, std::vector<uint8_t> data)
        : NetworkEvent(id), payload(std::move(data)) {}
};

struct SendPacketEvent : public NetworkEvent {
    std::vector<uint8_t> payload;

    SendPacketEvent() = default;
    SendPacketEvent(SessionId id, std::vector<uint8_t> data)
        : NetworkEvent(id), payload(std::move(data)) {}
};

struct PlayerJoinEvent : public NetworkEvent {
    std::string login;

    PlayerJoinEvent() = default;
    PlayerJoinEvent(SessionId id, std::string name)
        : NetworkEvent(id), login(std::move(name)) {}
};

struct PlayerMoveEvent : public NetworkEvent {
    double angle{0.0};

    PlayerMoveEvent() = default;
    PlayerMoveEvent(SessionId id, double a)
        : NetworkEvent(id), angle(a) {}
};

struct PlayerShootEvent : public NetworkEvent {
    PlayerShootEvent() = default;
    explicit PlayerShootEvent(SessionId id) : NetworkEvent(id) {}
};

struct PlayerRespawnEvent : public NetworkEvent {
    PlayerRespawnEvent() = default;
    explicit PlayerRespawnEvent(SessionId id) : NetworkEvent(id) {}
};

struct ShotMessageEvent : public NetworkEvent {
    ShotMessageEvent() = default;
    explicit ShotMessageEvent(SessionId id) : NetworkEvent(id) {}
};

struct ShipDestroyedEvent : public NetworkEvent {
    ShipDestroyedEvent() = default;
    explicit ShipDestroyedEvent(SessionId id) : NetworkEvent(id) {}
};

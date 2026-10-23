#include <iostream>

#include "event_bus.h"
#include "frame_codec.h"
#include "logger.h"
#include "network_context.h"
#include "network_response_router.h"
#include "tcp_server.h"
#include "world_instance.h"

int main() {
    try {
        NetworkContext nc;
        auto event_bus = events::EventBus::create();
        network::FrameCodec frame_codec;
        auto router = NetworkResponseRouter::create(event_bus);
        auto server = std::make_shared<TcpServer>(nc.getContext(), 8080, event_bus, frame_codec);

        auto world = WorldInstance::create(nc.getContext(), event_bus);
        nc.start();
        server->start();
        world->start();
        LOG_INFO("[Server] SpaceShooterC- Server Started.");

        std::cin.get();  // блокирует до enter

        server->stop();
        nc.stop();
        nc.wait();
        world->stop();

    } catch (const std::exception& e) {
        LOG_ERROR("[Server] Exception: {}", e.what());
        return 1;
    }
    return 0;
}

#include <iostream>
#include <future>
#include <boost/asio/signal_set.hpp>

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

        // Promise для блокировки основного потока до сигнала остановки
        std::promise<void> shutdown_promise;
        auto shutdown_future = shutdown_promise.get_future();

        // Асинхронный перехват Ctrl+C (SIGINT) и SIGTERM
        boost::asio::signal_set signals(nc.getContext(), SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code& /*ec*/, int /*sig*/) {
            LOG_INFO("[Server] Shutdown signal received, initiating stop...");
            try {
                shutdown_promise.set_value();
            } catch (...) {}
        });

        nc.start();
        server->start();
        world->start();
        LOG_INFO("[Server] SpaceShooterC- Server Started on port 8080. Press Ctrl+C to stop.");

        // Блокируем main здесь, пока не придет сигнал прерывания
        shutdown_future.wait();

        // Корректная последовательность остановки (stop перед wait):
        LOG_INFO("[Server] Stopping components...");
        server->stop();
        world->stop();
        nc.stop();
        nc.wait();

        LOG_INFO("[Server] Server stopped gracefully.");

    } catch (const std::exception& e) {
        LOG_ERROR("[Server] Exception: {}", e.what());
        return 1;
    }
    return 0;
}

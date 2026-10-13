#include <boost/asio.hpp>
#include <memory>

#include "game_controller.h"
#include "logger.h"
#include "tcp_client.h"

int main() {
    try {
        boost::asio::io_context io_context;
        auto tcp_client = std::make_shared<TcpClient>(io_context);
        auto game = client::GameController::create(tcp_client, io_context);

        const std::string host = "127.0.0.1";
        const std::uint16_t port = 8080;
        const std::string name = "TestName";

        game->start(host, port, name);

        LOG_INFO("Entering io_context.run()");
        io_context.run();
        LOG_INFO("io_context.run() returned");

        // Drain the shutdown handler queued by shutdown() so no strand handler
        // outlives the io_context below.
        game->shutdown();
        io_context.poll();
    } catch (const std::exception& e) {
        LOG_ERROR("[Client] exception: {}", e.what());
        return 1;
    }
    return 0;
}

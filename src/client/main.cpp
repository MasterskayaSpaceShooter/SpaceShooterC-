#include <boost/asio.hpp>
#include <memory>

#include "client_world.h"
#include "logger.h"
#include "null_input_provider.h"
#include "null_renderer.h"
#include "tcp_client.h"

int main() {
    try {
        boost::asio::io_context io_context;
        auto client = std::make_shared<TcpClient>(io_context);

        client->connect("127.0.0.1", 8080, [](bool status) {
            if (status) {
                LOG_INFO("[Client] Connected successfully.");
            } else {
                LOG_ERROR("[Client] Connection failed.");
            }
        });

        io_context.run();
    } catch (const std::exception& e) {
        LOG_ERROR("[Client] exception: {}", e.what());
        return 1;
    }
    return 0;
}

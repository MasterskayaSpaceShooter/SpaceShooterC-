#include <SFML/Graphics.hpp>
#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "game_controller.h"
#include "logger.h"
#include "sfml_input_provider.h"
#include "sfml_renderer.h"
#include "tcp_client.h"

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    uint16_t port = 8080;
    std::string name = "Pilot";

    if (argc >= 2) name = argv[1];
    if (argc >= 3) host = argv[2];
    if (argc >= 4) port = static_cast<uint16_t>(std::stoi(argv[3]));

    LOG_INFO("Starting client for player '{}', connecting to {}:{}", name, host, port);

    sf::RenderWindow window(sf::VideoMode(1024, 768), "Space Shooter - " + name);
    window.setFramerateLimit(60);

    auto renderer = std::make_shared<client::SfmlRenderer>(window);
    auto input_provider = std::make_shared<client::SfmlInputProvider>(window, *renderer);

    boost::asio::io_context io_context;
    auto tcp = std::make_shared<TcpClient>(io_context);

    std::promise<bool> connect_promise;
    auto connect_future = connect_promise.get_future();

    tcp->connect(host, port, [&connect_promise](bool success) {
        connect_promise.set_value(success);
    });

    std::thread net_thread([&io_context]() {
        io_context.run();
    });

    // Ожидаем фактического завершения рукопожатия TCP:
    if (connect_future.wait_for(std::chrono::seconds(5)) != std::future_status::ready || !connect_future.get()) {
        LOG_ERROR("Could not connect to server at {}:{}", host, port);
        tcp->disconnect();
        io_context.stop();
        if (net_thread.joinable()) net_thread.join();
        return 1;
    }

    LOG_INFO("TCP connection established. Entering game loop.");

    client::GameController controller(window, tcp, renderer, input_provider, name);
        controller.run();

    tcp->disconnect();
    io_context.stop();
    if (net_thread.joinable()) {
        net_thread.join();
    }

    return 0;
}


#include <boost/asio.hpp>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

#include "event_bus.h"
#include "frame_codec.h"
#include "network_events.h"
#include "tcp_server.h"

namespace {

using namespace std::chrono_literals;

uint16_t getFreePort() {
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::acceptor acceptor(io_context, {boost::asio::ip::tcp::v4(), 0});

    return acceptor.local_endpoint().port();
}

struct ServerFixture : testing::Test {
    boost::asio::io_context io_context;
    network::FrameCodec codec;
    std::shared_ptr<events::EventBus> event_bus = events::EventBus::create();

    uint16_t port = getFreePort();

    std::shared_ptr<TcpServer> server = std::make_shared<TcpServer>(io_context, port, event_bus, codec);

    std::thread server_thread;

    void startServer() {
        server->start();
        server_thread = std::thread([this] {
            io_context.run();
        });
    }

    void stopServer() {
        server->stop();
        io_context.stop();

        if (server_thread.joinable()) {
            server_thread.join();
        }
    }

    ~ServerFixture() override {
        stopServer();
    }

    boost::asio::ip::tcp::socket connectClient() {
        boost::asio::ip::tcp::socket socket(io_context);
        socket.connect({boost::asio::ip::address_v4::loopback(), port});
        return socket;
    }
};

TEST(TcpServerConstructorTest, ThrowsWhenEventBusIsNull) {
    boost::asio::io_context io_context;
    network::FrameCodec codec;

    EXPECT_THROW(TcpServer(io_context, 0, nullptr, codec), std::invalid_argument);
}

TEST_F(ServerFixture, ConstructorAcceptsValidEventBus) {
    EXPECT_NE(server, nullptr);
}

TEST_F(ServerFixture, AcceptsClientAndPublishesConnectedEvent) {
    std::mutex mutex;
    std::condition_variable condition;
    bool event_received = false;
    SessionId received_id = 0;
    std::string received_address;

    auto subscription = event_bus->subscribe<ClientConnectedEvent>([&](const ClientConnectedEvent& event) {
        {
            std::lock_guard lock(mutex);
            event_received = true;
            received_id = event.session_id;
            received_address = event.remote_address;
        }
        condition.notify_one();
    });

    startServer();
    auto client = connectClient();

    std::unique_lock lock(mutex);
    const bool received = condition.wait_for(lock, 2s, [&] {
        return event_received;
    });

    lock.unlock();
    subscription.disconnect();

    ASSERT_TRUE(received) << "Server should publish ClientConnectedEvent";

    EXPECT_EQ(received_id, 1);
    EXPECT_FALSE(received_address.empty());
    EXPECT_NE(received_address.find("127.0.0.1"), std::string::npos);
}

TEST_F(ServerFixture, AcceptsMultipleClientsWithUniqueSessionIds) {
    constexpr int client_count = 3;

    std::mutex mutex;
    std::condition_variable condition;
    std::set<SessionId> received_ids;

    auto subscription = event_bus->subscribe<ClientConnectedEvent>([&](const ClientConnectedEvent& event) {
        {
            std::lock_guard lock(mutex);
            received_ids.insert(event.session_id);
        }
        condition.notify_one();
    });

    startServer();

    std::vector<boost::asio::ip::tcp::socket> clients;
    for (int i = 0; i < client_count; ++i) {
        clients.push_back(connectClient());
    }

    std::unique_lock lock(mutex);
    const bool all_received = condition.wait_for(lock, 2s, [&] {
        return received_ids.size() == client_count;
    });

    lock.unlock();
    subscription.disconnect();

    ASSERT_TRUE(all_received) << "Server should accept every client";

    EXPECT_EQ(received_ids.size(), client_count);
    EXPECT_EQ(*received_ids.begin(), 1);
    EXPECT_EQ(*received_ids.rbegin(), client_count);
}

TEST_F(ServerFixture, StopWithoutStartDoesNotThrow) {
    EXPECT_NO_THROW(server->stop());
}

}  // namespace

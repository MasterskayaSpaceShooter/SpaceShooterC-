#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

#include "frame_codec.h"
#include "tcp_client.h"

namespace net = boost::asio;
using tcp = net::ip::tcp;

// Вспомогательный echo-сервер для тестов
class EchoServer {
public:
    EchoServer(net::io_context& ioc, uint16_t port) : acceptor_(ioc, tcp::endpoint(tcp::v4(), port)), socket_(ioc) {
        doAccept();
    }

    uint16_t port() const {
        return acceptor_.local_endpoint().port();
    }

private:
    void doAccept() {
        acceptor_.async_accept(socket_, [this](const boost::system::error_code& ec) {
            if (!ec) {
                doRead();
            }
        });
    }

    void doRead() {
        socket_.async_read_some(net::buffer(read_buf_), [this](const boost::system::error_code& ec, std::size_t n) {
            if (ec)
                return;
            net::write(socket_, net::buffer(read_buf_, n));
            doRead();
        });
    }

    tcp::acceptor acceptor_;
    tcp::socket socket_;
    std::array<uint8_t, 4096> read_buf_{};
};

// Вспомогательный сервер, который сразу закрывает соединение
class RejectServer {
public:
    RejectServer(net::io_context& ioc) : acceptor_(ioc, tcp::endpoint(tcp::v4(), 0)), socket_(ioc) {
        doAccept();
    }

    uint16_t port() const {
        return acceptor_.local_endpoint().port();
    }

private:
    void doAccept() {
        acceptor_.async_accept(socket_, [this](const boost::system::error_code& ec) {
            if (!ec) {
                boost::system::error_code ignored;
                socket_.close(ignored);
            }
        });
    }

    tcp::acceptor acceptor_;
    tcp::socket socket_;
};

class TcpClientTest : public ::testing::Test {
protected:
    net::io_context ioc_;
    network::FrameCodec codec_;
    std::shared_ptr<TcpClient> client_;
    std::unique_ptr<EchoServer> server_;
    std::thread io_thread_;

    void SetUp() override {
        server_ = std::make_unique<EchoServer>(ioc_, 0);
        client_ = std::make_shared<TcpClient>(ioc_, codec_);
        io_thread_ = std::thread([this] {
            ioc_.run();
        });
    }

    void TearDown() override {
        client_->disconnect();
        ioc_.stop();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
    }

    bool waitFor(std::function<bool()> cond, int timeout_ms = 1000) {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(timeout_ms)) {
            if (cond())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return cond();
    }
};

// Тесты подключения

TEST_F(TcpClientTest, ConnectSuccess) {
    std::atomic<bool> connected{false};

    client_->connect("127.0.0.1", server_->port(), [&connected](bool ok) {
        connected = ok;
    });

    EXPECT_TRUE(waitFor([&] {
        return connected.load();
    }));
    EXPECT_TRUE(client_->isConnected());
}

TEST_F(TcpClientTest, ConnectWithoutCallback) {
    client_->connect("127.0.0.1", server_->port(), {});

    EXPECT_TRUE(waitFor([&] {
        return client_->isConnected();
    }));
}

TEST_F(TcpClientTest, DisconnectFromConnectCallbackDoesNotStartRead) {
    std::atomic<bool> disconnected{false};
    client_->setDisconnectCallback([&disconnected] {
        disconnected = true;
    });

    client_->connect("127.0.0.1", server_->port(), [this](bool connected) {
        if (connected) {
            client_->disconnect();
        }
    });

    EXPECT_TRUE(waitFor([&] {
        return disconnected.load();
    }));
    EXPECT_FALSE(client_->isConnected());
}

TEST_F(TcpClientTest, SendBeforeConnectDoesNotPreventConnection) {
    client_->send({1, 2, 3});

    std::atomic<bool> connected{false};
    client_->connect("127.0.0.1", server_->port(), [&connected](bool ok) {
        connected = ok;
    });

    EXPECT_TRUE(waitFor([&] {
        return connected.load();
    }));
}

TEST_F(TcpClientTest, ConnectToInvalidHost) {
    std::atomic<bool> result{false};
    std::atomic<bool> callback_called{false};

    client_->connect("999.999.999.999", 12345, [&result, &callback_called](bool ok) {
        result = ok;
        callback_called = true;
    });

    waitFor([&] {
        return callback_called.load();
    });
    EXPECT_FALSE(result);
    EXPECT_FALSE(client_->isConnected());
}

TEST_F(TcpClientTest, DoubleConnectFails) {
    std::atomic<bool> first_done{false};
    std::atomic<bool> second{false};

    client_->connect("127.0.0.1", server_->port(), [&first_done](bool /*ok*/) {
        first_done = true;
    });

    EXPECT_TRUE(waitFor(
        [&] {
            return client_->isConnected();
        },
        2000));

    client_->connect("127.0.0.1", server_->port(), [&second](bool ok) {
        second = ok;
    });

    EXPECT_FALSE(second.load());
}

TEST(TcpClientCancellationTest, DisconnectDuringConnectCompletesCallbackOnce) {
    net::io_context ioc;
    network::FrameCodec codec;
    auto client = std::make_shared<TcpClient>(ioc, codec);
    tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), 0));
    std::atomic<int> callback_count{0};
    std::atomic<bool> callback_result{true};

    client->connect("127.0.0.1", acceptor.local_endpoint().port(), [&](bool connected) {
        callback_result = connected;
        ++callback_count;
    });

    ASSERT_EQ(ioc.run_one(), 1U);
    ASSERT_EQ(ioc.run_one(), 1U);

    client->disconnect();
    ioc.run();

    EXPECT_EQ(callback_count.load(), 1);
    EXPECT_FALSE(callback_result.load());
    EXPECT_FALSE(client->isConnected());
}

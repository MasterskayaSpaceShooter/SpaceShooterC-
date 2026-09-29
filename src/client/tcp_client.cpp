#include "tcp_client.h"

namespace net = boost::asio;
using con = net::io_context&;
using tcp = net::ip::tcp;

TcpClient::TcpClient(con io_context, FrameCodec& codec) : socket_(io_context), codec_(codec) {
    read_buffer_.resize(READ_BLOCK_SIZE);
}

TcpClient::~TcpClient() {
    disconnect();
}

void TcpClient::connect(const std::string& host, uint16_t port, std::function<void(bool)> on_connect) {
    if (is_connected_) {
        on_connect(false);
        return;
    }

    auto client_ptr = shared_from_this();  // для удеражания клиента

    boost::system::error_code ec;

    auto address = net::ip::address::from_string(host, ec);  // для поддержки IPv4 и IPv6

    if (ec) {
        on_connect(false);
        return;
    }

    auto endpoint = tcp::endpoint(address, port);

    boost::asio::async_connect(
        client_ptr->socket_,
        std::vector<tcp::endpoint>{endpoint},
        [client_ptr, on_connect](const boost::system::error_code& ec2, const tcp::endpoint& /*ep*/) {
            if (!ec2) {
                client_ptr->is_connected_ = true;
                on_connect(true);
                client_ptr->doRead();
            } else {
                client_ptr->is_connected_ = false;
                on_connect(false);
            }
        });
}

void TcpClient::send(std::vector<uint8_t> data) {
    std::lock_guard<std::mutex> lock_write_mutex(write_mutex_);
    write_queue_.push(std::move(data));

    if (!is_writing_) {
        is_writing_ = true;
        doWrite();
    }
}

void TcpClient::disconnect() {
    if (!is_connected_) {
        return;
    }

    boost::system::error_code ec;
    is_connected_ = false;
    socket_.close(ec);

    // Очищаем очередь
    {
        std::lock_guard<std::mutex> lock_write_mutex(write_mutex_);
        std::queue<std::vector<uint8_t>> empty;
        write_queue_.swap(empty);
        is_writing_ = false;
    }

    // Вызоваем пользовательский колбэк
    if (disconnect_cb_) {
        disconnect_cb_();
    }
}

void TcpClient::doRead() {}

void TcpClient::doWrite() {}

#include "tcp_client.h"

#include "frame_codec.h"
#include "logger.h"

namespace net = boost::asio;
using con = net::io_context&;
using tcp = net::ip::tcp;

TcpClient::TcpClient(con io_context, network::FrameCodec& codec) : socket_(io_context), codec_(codec) {
    read_buffer_.reserve(READ_BLOCK_SIZE);
    LOG_INFO("TCPclient created");
}

TcpClient::~TcpClient() {
    disconnect();
    LOG_INFO("TcpClient destroyed");
}

void TcpClient::connect(const std::string& host, uint16_t port, std::function<void(bool)> on_connect) {
    if (is_connected_) {
        on_connect(false);
        LOG_INFO("TcpClient is already connected");
        return;
    }

    auto client_ptr = shared_from_this();  // для удеражания клиента

    boost::system::error_code ec;

    auto address = net::ip::address::from_string(host, ec);  // для поддержки IPv4 и IPv6

    if (ec) {
        on_connect(false);
        LOG_ERROR("Invalid host address: {}", host);
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
                LOG_ERROR("Connection failed: {}", ec2.message());
                on_connect(false);
            }
        });

    LOG_INFO("TcpClient is connect");
}

void TcpClient::send(std::vector<uint8_t> data) {
    bool should_start_write = false;
    {
        std::lock_guard<std::mutex> lock_write_mutex(write_mutex_);
        write_queue_.push(std::move(data));

        if (!is_writing_) {
            is_writing_ = true;
            should_start_write = true;
        }
    }
    if (should_start_write) {
        doWrite();
    }
    LOG_INFO("Message sent");
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

void TcpClient::doRead() {
    auto client_ptr = shared_from_this();
    // Резервируем место в буффер(Есть ли смысл ? если зарезервировано уже в конструкторе)
    // Читаем байты
    // Фиксируем прочитанные байты
    // Вызывем декодор FrameCode, парсим кадры
    client_ptr->socket_.async_read_some(
        client_ptr->read_buffer_.prepare(READ_BLOCK_SIZE),
        [client_ptr](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            if (ec) {
                LOG_ERROR("Can not read buffer");
                client_ptr->disconnect();
                return;
            }
            client_ptr->read_buffer_.commit(bytes_transferred);
            network::FrameCodec::decode(client_ptr->read_buffer_, [&client_ptr](network::PayloadView payload) {
                if (client_ptr->message_cb_) {
                    std::vector<std::uint8_t> frame_data(payload.begin(), payload.end());
                    client_ptr->message_cb_(frame_data);
                }
            });

            client_ptr->doRead();
        });
}

void TcpClient::doWrite() {
    std::vector<uint8_t> payload;
    {
        std::lock_guard<std::mutex> lock_write_mutex(write_mutex_);
        if (write_queue_.empty()) {
            is_writing_ = false;
            return;
        }

        payload = std::move(write_queue_.front());
        write_queue_.pop();
    }

    auto client_ptr = shared_from_this();
    auto frame = network::FrameCodec::encode(std::move(payload));

    boost::asio::async_write(
        socket_,
        frame.buffers(),
        [client_ptr, frame = std::move(frame)](const boost::system::error_code& ec, std::size_t /*bytes*/) {
            if (ec) {
                LOG_ERROR("Write error: {}", ec.message());
                client_ptr->disconnect();
                return;
            }
            std::lock_guard<std::mutex> lock_write_mutex(client_ptr->write_mutex_);
            if (!client_ptr->write_queue_.empty()) {
                client_ptr->doWrite();
            } else {
                client_ptr->is_writing_ = false;
            }
        });
}

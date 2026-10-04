#include "tcp_client.h"

#include "frame_codec.h"
#include "logger.h"

namespace net = boost::asio;
using con = net::io_context&;
using tcp = net::ip::tcp;

TcpClient::TcpClient(con io_context, network::FrameCodec& codec) :
    strand_(boost::asio::make_strand(io_context)), socket_(strand_), codec_(codec) {
    read_buffer_.reserve(READ_BLOCK_SIZE);
    LOG_INFO("TCPclient created");
}

TcpClient::~TcpClient() {
    // disconnect();
    boost::system::error_code ec;
    socket_.cancel(ec);
    socket_.close(ec);

    LOG_INFO("TcpClient destroyed");
}

void TcpClient::connect(const std::string& host, uint16_t port, std::function<void(bool)> on_connect) {
    auto client_ptr = shared_from_this();  // для удеражания клиента
    // Используем disconnect_generation_ для предотвращения гонок при повторных подключениях
    const auto generation = client_ptr->disconnect_generation_.load(std::memory_order_acquire);

    net::post(strand_, [client_ptr, host, port, generation, on_connect = std::move(on_connect)]() mutable {
        if (generation != client_ptr->disconnect_generation_.load(std::memory_order_acquire)) {
            if (on_connect) {
                on_connect(false);
            }
            return;
        }

        if (client_ptr->is_connected_.load(std::memory_order_acquire) || client_ptr->is_connecting_) {
            if (on_connect) {
                on_connect(false);
            }
            LOG_INFO("TcpClient is already connected or connecting");
            return;
        }
        // Устанавливаем флаг подключения и сохраняем колбэк
        // Создаем резолвер для асинхронного разрешения имени хоста
        // Используем std::move для перемещения колбэка в pending_connect_callback_
        // Используем std::make_shared для создания резолвера в куче
        // Используем async_resolve для асинхронного разрешения имени хоста и порта

        client_ptr->is_connecting_ = true;
        client_ptr->active_connect_generation_ = generation;
        client_ptr->pending_connect_callback_ = std::move(on_connect);
        client_ptr->resolver_ = std::make_shared<tcp::resolver>(client_ptr->strand_);

        client_ptr->resolver_->async_resolve(
            host,
            std::to_string(port),
            net::bind_executor(
                client_ptr->strand_,
                [client_ptr, generation](const boost::system::error_code& ec, tcp::resolver::results_type endpoints) {
                    if (generation != client_ptr->active_connect_generation_ || !client_ptr->is_connecting_ ||
                        generation != client_ptr->disconnect_generation_.load(std::memory_order_acquire)) {
                        return;
                    }

                    client_ptr->resolver_.reset();
                    if (ec) {
                        client_ptr->is_connecting_ = false;
                        auto callback = std::move(client_ptr->pending_connect_callback_);
                        LOG_ERROR("Invalid host address: {}", ec.message());
                        if (callback) {
                            callback(false);
                        }
                        return;
                    }

                    net::async_connect(
                        client_ptr->socket_,
                        endpoints,
                        net::bind_executor(
                            client_ptr->strand_,
                            [client_ptr, generation](const boost::system::error_code& connect_ec,
                                                     const tcp::endpoint& /*endpoint*/) {
                                if (generation != client_ptr->active_connect_generation_ ||
                                    !client_ptr->is_connecting_ ||
                                    generation != client_ptr->disconnect_generation_.load(std::memory_order_acquire)) {
                                    return;
                                }

                                client_ptr->is_connecting_ = false;
                                auto callback = std::move(client_ptr->pending_connect_callback_);
                                if (connect_ec) {
                                    client_ptr->is_connected_.store(false, std::memory_order_release);
                                    LOG_ERROR("Connection failed: {}", connect_ec.message());
                                    if (callback) {
                                        callback(false);
                                    }
                                    return;
                                }

                                client_ptr->is_connected_.store(true, std::memory_order_release);
                                LOG_INFO("TcpClient connected");
                                if (callback) {
                                    callback(true);
                                }
                                client_ptr->doRead();
                            }));
                    LOG_INFO("TcpClient is connect");
                }));
    });
}

void TcpClient::send(std::vector<uint8_t> data) {
    auto client_ptr = shared_from_this();

    // Используем strand для обеспечения последовательного выполнения операций записи
    // Перемещаем данные в лямбдуб чтобы избежать копирования
    // Используем std::move для перемещения данных в очередь
    // Если клиент уже пишет, то просто добавляем в очередь, иначе начинаем писать
    net::post(strand_, [client_ptr, message = std::move(data)]() {
        if (!client_ptr->is_connected_.load()) {
            LOG_INFO("TcpClient is not connected, dropping message");
            return;
        }

        client_ptr->write_queue_.push(std::move(message));
        if (!client_ptr->is_writing_) {
            client_ptr->is_writing_ = true;
            client_ptr->doWrite();
        }
    });
}

void TcpClient::disconnect() {
    auto client_ptr = shared_from_this();

    client_ptr->disconnect_generation_.fetch_add(1, std::memory_order_acq_rel);
    const bool was_connected = client_ptr->is_connected_.exchange(false, std::memory_order_acq_rel);

    // Используем strand для обеспечения последовательного выполнения операций отключения
    // Закрываем сокет и очищаем очередь сообщений
    // Вызываем пользовательский колбэк
    net::post(strand_, [client_ptr, was_connected]() {
        const bool was_connecting = client_ptr->is_connecting_;
        if (!was_connected && !was_connecting) {
            LOG_INFO("TcpClient is already disconnected");
            return;
        }

        client_ptr->is_connecting_ = false;
        if (client_ptr->resolver_) {
            client_ptr->resolver_->cancel();
            client_ptr->resolver_.reset();
        }

        boost::system::error_code ec;
        client_ptr->socket_.cancel(ec);
        client_ptr->socket_.close(ec);
        std::queue<std::vector<uint8_t>> empty;
        client_ptr->write_queue_.swap(empty);
        client_ptr->is_writing_ = false;
        LOG_INFO("TcpClient disconnected");

        auto connect_callback = std::move(client_ptr->pending_connect_callback_);
        if (was_connecting && connect_callback) {
            connect_callback(false);
        }

        // Вызоваем пользовательский колбэк
        if (was_connected && client_ptr->disconnect_cb_) {
            client_ptr->disconnect_cb_();
        }
    });
}

void TcpClient::doRead() {
    auto client_ptr = shared_from_this();
    if (!client_ptr->is_connected_.load(std::memory_order_acquire)) {
        return;
    }

    // Резервируем место в буффер(Есть ли смысл ? если зарезервировано уже в конструкторе)
    // Читаем байты асинхронно
    // Фиксируем прочитанные байты
    // Вызывем декодор FrameCode, парсим кадры
    client_ptr->socket_.async_read_some(
        client_ptr->read_buffer_.prepare(READ_BLOCK_SIZE),
        net::bind_executor(
            client_ptr->strand_,
            [client_ptr](const boost::system::error_code& ec, std::size_t bytes_transferred) {
                if (ec) {
                    LOG_ERROR("Can not read buffer");
                    client_ptr->disconnect();
                    return;
                }
                client_ptr->read_buffer_.commit(bytes_transferred);
                network::FrameCodec::decode(client_ptr->read_buffer_, [client_ptr](network::PayloadView payload) {
                    if (client_ptr->message_cb_) {
                        std::vector<std::uint8_t> frame_data(payload.begin(), payload.end());
                        client_ptr->message_cb_(frame_data);
                    }
                });

                if (!client_ptr->is_connected_.load(std::memory_order_acquire)) {
                    return;
                }
                client_ptr->doRead();
            }));
}

void TcpClient::doWrite() {
    // Проверяем, есть ли данные в очереди на запись

    std::vector<uint8_t> payload;
    if (write_queue_.empty()) {
        is_writing_ = false;
        return;
    }
    // Берем данные из очереди на запись и удаляем их из очереди
    // Используем std::move для перемещения данных в payload
    // Кодировка данных в кадр с помощью FrameCodec
    // Асинхронная запись данных в сокет
    payload = std::move(write_queue_.front());
    write_queue_.pop();

    auto client_ptr = shared_from_this();
    auto frame = std::make_shared<network::FrameCodec::Frame>(network::FrameCodec::encode(std::move(payload)));
    const auto buffers = frame->buffers();

    boost::asio::async_write(
        socket_,
        buffers,
        net::bind_executor(
            strand_,
            [client_ptr, frame = std::move(frame)](const boost::system::error_code& ec, std::size_t /*bytes*/) {
                if (ec) {
                    LOG_ERROR("Write error: {}", ec.message());
                    client_ptr->disconnect();
                    return;
                }

                if (!client_ptr->write_queue_.empty()) {
                    client_ptr->doWrite();
                } else {
                    client_ptr->is_writing_ = false;
                }
            }));
}

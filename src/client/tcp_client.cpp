#include "tcp_client.h"

#include "frame_codec.h"
#include "logger.h"

namespace net = boost::asio;
using con = net::io_context&;
using tcp = net::ip::tcp;

TcpClient::TcpClient(con io_context) : strand_(boost::asio::make_strand(io_context)), socket_(strand_) {
    read_buffer_.reserve(READ_BLOCK_SIZE);
    LOG_INFO("TCPclient created");
}

TcpClient::~TcpClient() {
    boost::system::error_code ec;
    socket_.cancel(ec);
    socket_.close(ec);
    LOG_INFO("TcpClient destroyed");
}

void TcpClient::connect(const std::string& host, uint16_t port, std::function<void(bool)> on_connect) {
    try {
        auto client_ptr = shared_from_this();
        auto connect_cb = std::make_shared<std::function<void(bool)>>(std::move(on_connect));

        // Отправляем задачу на подключение в strand, чтобы избежать гонок
        // Внутри strand проверяем состояние соединения и выполняем подключение
        // Если соединение уже активно или в процессе подключения, вызываем callback с false
        net::post(strand_, [client_ptr, host, port, connect_cb]() mutable {
            try {
                const auto generation = client_ptr->disconnect_generation_;
                if (client_ptr->is_connected_.load(std::memory_order_acquire) || client_ptr->is_connecting_) {
                    if (*connect_cb) {
                        try {
                            (*connect_cb)(false);
                        } catch (const std::exception& ex) {
                            LOG_ERROR("on_connect threw: {}", ex.what());
                        } catch (...) {
                            LOG_ERROR("on_connect threw unknown");
                        }
                    }
                    return;
                }

                // Устанавливаем флаг, что соединение в процессе подключения
                // Сохраняем callback для вызова после завершения подключения\
                // Создаем resolver для асинхронного разрешения имени хоста
                // Запускаем async_resolve, передавая callback для обработки результата
                // В callback проверяем, что соединение все еще активно и не было запроса на отключение
                client_ptr->disconnect_requested_.store(false, std::memory_order_release);
                client_ptr->is_connecting_ = true;
                client_ptr->active_connect_generation_ = generation;
                client_ptr->pending_connect_callback_ = std::move(*connect_cb);
                client_ptr->resolver_ = std::make_shared<tcp::resolver>(client_ptr->strand_);

                // Запускаем асинхронное разрешение имени хоста
                // Если разрешение успешно, запускаем асинхронное подключение к первому доступному endpoint
                // Если разрешение не удалось, вызываем callback с false и очищаем resolver
                client_ptr->resolver_->async_resolve(
                    host,
                    std::to_string(port),
                    net::bind_executor(
                        client_ptr->strand_,
                        [client_ptr, generation](const boost::system::error_code& ec,
                                                 tcp::resolver::results_type endpoints) noexcept {
                            try {
                                if (generation != client_ptr->active_connect_generation_ ||
                                    !client_ptr->is_connecting_ || generation != client_ptr->disconnect_generation_) {
                                    return;
                                }

                                client_ptr->resolver_.reset();
                                if (ec) {
                                    client_ptr->is_connecting_ = false;
                                    auto callback = std::move(client_ptr->pending_connect_callback_);
                                    LOG_ERROR("Invalid host address: {}", ec.message());
                                    if (callback) {
                                        try {
                                            callback(false);
                                        } catch (const std::exception& ex) {
                                            LOG_ERROR("on_connect threw: {}", ex.what());
                                        } catch (...) {
                                            LOG_ERROR("on_connect threw unknown");
                                        }
                                    }
                                    return;
                                }
                                // Запускаем асинхронное подключение к первому доступному endpoint
                                // В callback проверяем, что соединение все еще активно и не было запроса на отключение
                                // Если подключение успешно, вызываем callback с true, иначе с false
                                // Если подключение успешно, запускаем doRead для чтения данных
                                // Если произошла ошибка, отключаемся
                                net::async_connect(
                                    client_ptr->socket_,
                                    endpoints,
                                    net::bind_executor(
                                        client_ptr->strand_,
                                        [client_ptr, generation](const boost::system::error_code& connect_ec,
                                                                 const tcp::endpoint&) noexcept {
                                            try {
                                                if (generation != client_ptr->active_connect_generation_ ||
                                                    !client_ptr->is_connecting_ ||
                                                    generation != client_ptr->disconnect_generation_) {
                                                    return;
                                                }

                                                client_ptr->is_connecting_ = false;
                                                auto callback = std::move(client_ptr->pending_connect_callback_);
                                                if (connect_ec) {
                                                    client_ptr->is_connected_.store(false, std::memory_order_release);
                                                    LOG_ERROR("Connection failed: {}", connect_ec.message());
                                                    if (callback) {
                                                        try {
                                                            callback(false);
                                                        } catch (const std::exception& ex) {
                                                            LOG_ERROR("on_connect threw: {}", ex.what());
                                                        } catch (...) {
                                                            LOG_ERROR("on_connect threw unknown");
                                                        }
                                                    }
                                                    return;
                                                }

                                                client_ptr->is_connected_.store(true, std::memory_order_release);
                                                LOG_INFO("TcpClient connected");
                                                if (callback) {
                                                    try {
                                                        callback(true);
                                                    } catch (const std::exception& ex) {
                                                        LOG_ERROR("on_connect threw: {}", ex.what());
                                                    } catch (...) {
                                                        LOG_ERROR("on_connect threw unknown");
                                                    }
                                                }
                                                client_ptr->doRead();
                                            } catch (const std::exception& ex) {
                                                LOG_ERROR("Exception in connect completion handler: {}", ex.what());
                                                client_ptr->disconnect();
                                            } catch (...) {
                                                LOG_ERROR("Unknown exception in connect completion handler");
                                                client_ptr->disconnect();
                                            }
                                        }));
                            } catch (const std::exception& ex) {
                                LOG_ERROR("Exception in resolve handler: {}", ex.what());
                                client_ptr->disconnect();

                            } catch (...) {
                                LOG_ERROR("Unknown exception in resolve handler");
                                client_ptr->disconnect();
                            }
                        }));
            } catch (const std::exception& ex) {
                LOG_ERROR("Exception in connect handler: {}", ex.what());
                client_ptr->disconnect();
            } catch (...) {
                LOG_ERROR("Unknown exception in connect handler");
                client_ptr->disconnect();
            }
        });
    } catch (const std::exception& ex) {
        LOG_ERROR("Exception while scheduling connect: {}", ex.what());
    } catch (...) {
        LOG_ERROR("Unknown exception while scheduling connect");
    }
}

void TcpClient::send(std::vector<uint8_t> data) {
    try {
        if (data.size() > network::FrameCodec::kMaxMessageSize) {
            LOG_ERROR("Send rejected: payload exceeds maximum frame size");
            return;
        }

        auto frame = std::make_shared<network::FrameCodec::Frame>(network::FrameCodec::encode(std::move(data)));
        auto client_ptr = shared_from_this();

        net::post(strand_, [client_ptr, frame = std::move(frame)]() mutable {
            try {
                if (!client_ptr->is_connected_.load(std::memory_order_acquire) ||
                    client_ptr->disconnect_requested_.load(std::memory_order_acquire)) {
                    LOG_INFO("TcpClient is not connected, dropping message");
                    return;
                }

                client_ptr->write_queue_.push(std::move(frame));
                if (!client_ptr->is_writing_) {
                    client_ptr->doWrite();
                }
            } catch (const std::exception& ex) {
                LOG_ERROR("Exception in send handler: {}", ex.what());
            } catch (...) {
                LOG_ERROR("Unknown exception in send handler");
            }
        });
    } catch (const std::exception& ex) {
        LOG_ERROR("Exception while scheduling send: {}", ex.what());
    } catch (...) {
        LOG_ERROR("Unknown exception while scheduling send");
    }
}

void TcpClient::disconnect() noexcept {
    // Отправляем запрос на отключение в strand, чтобы избежать гонок
    try {
        auto client_ptr = weak_from_this().lock();
        if (!client_ptr) {
            LOG_ERROR("TcpClient::disconnect called without shared ownership");
            return;
        }
        // Устанавливаем флаг запроса на отключение
        // Внутри strand проверяем состояние соединения и выполняем отключение
        // Если соединение уже закрыто, то просто выходим
        // Если соединение активно, то закрываем сокет, очищаем буферы и вызываем callback
        // Если соединение было в процессе подключения, то вызываем callback с ошибкой
        client_ptr->disconnect_requested_.store(true, std::memory_order_release);
        net::post(strand_, [client_ptr]() {
            try {
                ++client_ptr->disconnect_generation_;
                const bool was_connected = client_ptr->is_connected_.exchange(false, std::memory_order_acq_rel);
                const bool was_connecting = client_ptr->is_connecting_;
                const bool has_pending_writes = !client_ptr->write_queue_.empty();
                if (!was_connected && !was_connecting && !has_pending_writes) {
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

                client_ptr->read_buffer_.clear();

                std::queue<std::shared_ptr<network::FrameCodec::Frame>> pending_writes;
                client_ptr->write_queue_.swap(pending_writes);
                client_ptr->is_writing_ = false;
                LOG_INFO("TcpClient disconnected");

                auto connect_callback = std::move(client_ptr->pending_connect_callback_);
                if (was_connecting && connect_callback) {
                    try {
                        connect_callback(false);
                    } catch (const std::exception& ex) {
                        LOG_ERROR("on_connect threw during disconnect: {}", ex.what());
                    } catch (...) {
                        LOG_ERROR("on_connect threw unknown during disconnect");
                    }
                }

                if (was_connected && client_ptr->disconnect_cb_) {
                    try {
                        client_ptr->disconnect_cb_();
                    } catch (const std::exception& ex) {
                        LOG_ERROR("disconnect callback threw: {}", ex.what());
                    } catch (...) {
                        LOG_ERROR("disconnect callback threw unknown");
                    }
                }
            } catch (const std::exception& ex) {
                LOG_ERROR("Exception in disconnect handler: {}", ex.what());
            } catch (...) {
                LOG_ERROR("Unknown exception in disconnect handler");
            }
        });
    } catch (const std::exception& ex) {
        LOG_ERROR("Exception while scheduling disconnect: {}", ex.what());
    } catch (...) {
        LOG_ERROR("Unknown exception while scheduling disconnect");
    }
}

void TcpClient::doRead() {
    auto client_ptr = shared_from_this();
    if (!client_ptr->is_connected_.load(std::memory_order_acquire) ||
        client_ptr->disconnect_requested_.load(std::memory_order_acquire)) {
        return;
    }
    const auto generation = client_ptr->active_connect_generation_;
    auto read_chunk = std::make_shared<std::array<std::uint8_t, READ_BLOCK_SIZE>>();

    // Резервируем место в буффер
    // Читаем байты асинхронно
    // Фиксируем прочитанные байты
    // Вызывем декодор FrameCode, парсим кадры
    // Если кадр прочитан полностью, то вызываем callback
    // Если произошла ошибка, то отключаемся
    try {
        client_ptr->socket_.async_read_some(
            net::buffer(*read_chunk),
            net::bind_executor(
                client_ptr->strand_,
                [client_ptr, read_chunk, generation](const boost::system::error_code& ec,
                                                     std::size_t bytes_transferred) noexcept {
                    try {
                        if (generation != client_ptr->active_connect_generation_ ||
                            generation != client_ptr->disconnect_generation_ ||
                            !client_ptr->is_connected_.load(std::memory_order_acquire) ||
                            client_ptr->disconnect_requested_.load(std::memory_order_acquire)) {
                            return;
                        }

                        if (ec) {
                            if (ec != boost::asio::error::operation_aborted) {
                                LOG_ERROR("Can not read buffer: {}", ec.message());
                            }
                            client_ptr->disconnect();
                            return;
                        }

                        auto writable = client_ptr->read_buffer_.prepare(bytes_transferred);
                        net::buffer_copy(writable, net::buffer(*read_chunk, bytes_transferred));
                        client_ptr->read_buffer_.commit(bytes_transferred);

                        bool stop_dispatch = false;
                        const auto decoded_frames = network::FrameCodec::decode(
                            client_ptr->read_buffer_,
                            [client_ptr, &stop_dispatch](network::PayloadView payload) {
                                if (stop_dispatch ||
                                    client_ptr->disconnect_requested_.load(std::memory_order_acquire)) {
                                    stop_dispatch = true;
                                    return;
                                }
                                if (client_ptr->message_cb_) {
                                    std::vector<std::uint8_t> frame_data(payload.begin(), payload.end());
                                    client_ptr->message_cb_(frame_data);
                                    if (client_ptr->disconnect_requested_.load(std::memory_order_acquire)) {
                                        stop_dispatch = true;
                                    }
                                }
                            });

                        (void)decoded_frames;
                        if (!client_ptr->is_connected_.load(std::memory_order_acquire) ||
                            client_ptr->disconnect_requested_.load(std::memory_order_acquire)) {
                            return;
                        }
                        client_ptr->doRead();
                    } catch (const std::exception& ex) {
                        LOG_ERROR("Exception in read handler : {}", ex.what());
                        client_ptr->disconnect();
                    } catch (...) {
                        LOG_ERROR("Unknown xception in read handler");
                        client_ptr->disconnect();
                    }
                }));
    } catch (const std::exception& ex) {
        // Синхронный выброс из async_read_some (напр., сокет уже закрыт)
        LOG_ERROR("Exception in doRead : {}", ex.what());
        disconnect();
    } catch (...) {
        LOG_ERROR("Unknown exception in doRead");
        disconnect();
    }
}

void TcpClient::doWrite() {
    // Если уже идет запись, то выходим
    // Если очередь пустая, то выходим
    try {
        if (!is_connected_.load(std::memory_order_acquire) || disconnect_requested_.load(std::memory_order_acquire)) {
            is_writing_ = false;
            return;
        }
        if (write_queue_.empty()) {
            is_writing_ = false;
            return;
        }

        // Берем первый элемент из очереди и начинаем асинхронную запись
        auto client_ptr = shared_from_this();
        const auto frame = write_queue_.front();
        const auto generation = active_connect_generation_;
        const auto buffers = frame->buffers();

        // Устанавливаем флаг, что идет запись
        // Асинхронно записываем данные в сокет
        // В обработчике проверяем, что соединение все еще активно
        // При успешной записи кадр удаляется из очереди.
        // Если в очереди есть еще элементы, то вызываем doWrite снова
        // Если произошла ошибка, то отключаемся

        is_writing_ = true;
        boost::asio::async_write(
            socket_,
            buffers,
            net::bind_executor(
                strand_,
                [client_ptr, frame, generation](const boost::system::error_code& ec, std::size_t /*bytes*/) noexcept {
                    try {
                        if (generation != client_ptr->active_connect_generation_ ||
                            generation != client_ptr->disconnect_generation_ ||
                            !client_ptr->is_connected_.load(std::memory_order_acquire)) {
                            return;
                        }

                        if (ec) {
                            LOG_ERROR("Write error: {}", ec.message());
                            client_ptr->disconnect();
                            return;
                        }

                        if (!client_ptr->write_queue_.empty() && client_ptr->write_queue_.front() == frame) {
                            client_ptr->write_queue_.pop();
                        }

                        if (!client_ptr->write_queue_.empty() &&
                            client_ptr->is_connected_.load(std::memory_order_acquire)) {
                            client_ptr->doWrite();
                        } else {
                            client_ptr->is_writing_ = false;
                        }
                    } catch (const std::exception& ex) {
                        LOG_ERROR("Exception in write handler: {}", ex.what());
                        client_ptr->disconnect();
                    } catch (...) {
                        LOG_ERROR("Unknown exception in write handler");
                        client_ptr->disconnect();
                    }
                }));
    } catch (const std::exception& ex) {
        LOG_ERROR("Exception in doWrite setup: {}", ex.what());
        is_writing_ = false;
        std::queue<std::shared_ptr<network::FrameCodec::Frame>> rejected_writes;
        rejected_writes.swap(write_queue_);
    } catch (...) {
        LOG_ERROR("Unknown exception in doWrite setup");
        is_writing_ = false;
        std::queue<std::shared_ptr<network::FrameCodec::Frame>> rejected_writes;
        rejected_writes.swap(write_queue_);
    }
}

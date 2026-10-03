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
    if (is_connected_) {
        on_connect(false);
        LOG_INFO("TcpClient is already connected");
        return;
    }

    auto client_ptr = shared_from_this();  // для удеражания клиента

    // Создаем резолвер и асинхронно разрешаем хост и порт
    // Если разрешение прошло успешно, подключаемся к первому доступному endpoint
    // Если подключение прошло успешно,  начинаем читать данные

    auto resolver = std::make_shared<tcp::resolver>(strand_);

    resolver->async_resolve(
        host,
        std::to_string(port),
        net::bind_executor(strand_,
                           [client_ptr, resolver, on_connect](const boost::system::error_code& ec,
                                                              tcp::resolver::results_type endpoint) {
                               if (ec) {
                                   on_connect(false);
                                   LOG_ERROR("Invalid host address: {}", ec.message());
                                   return;
                               }

                               // Подключаемся к первому доступному endpoint
                               net::async_connect(
                                   client_ptr->socket_,
                                   endpoint,
                                   net::bind_executor(client_ptr->strand_,
                                                      [client_ptr, on_connect](const boost::system::error_code& ec2,
                                                                               const tcp::endpoint& /*ep*/) {
                                                          if (!ec2) {
                                                              client_ptr->is_connected_ = true;
                                                              LOG_INFO("TcpClient connected");
                                                              on_connect(true);
                                                              client_ptr->doRead();
                                                          } else {
                                                              client_ptr->is_connected_ = false;
                                                              LOG_ERROR("Connection failed: {}", ec2.message());
                                                              on_connect(false);
                                                          }
                                                      }));
                               LOG_INFO("TcpClient is connect");
                           }));
}

void TcpClient::send(std::vector<uint8_t> data) {
    auto client_ptr = shared_from_this();

    // Используем strand для обеспечения последовательного выполнения операций записи
    // Перемещаем данные в лямбдуб чтобы избежать копирования
    // Используем std::move для перемещения данных в очередь
    // Если клиент уже пишет, то просто добавляем в очередь, иначе начинаем писать
    net::post(strand_, [client_ptr, message = std::move(data)]() {
        client_ptr->write_queue_.push(std::move(message));
        if (!client_ptr->is_writing_) {
            client_ptr->is_writing_ = true;
            client_ptr->doWrite();
        }
    });
    LOG_INFO("Message sent");
}

void TcpClient::disconnect() {
    auto client_ptr = shared_from_this();

    // Используем strand для обеспечения последовательного выполнения операций отключения
    // Проверяем, подключен ли клиент, если нет, то просто выходим
    // Закрываем сокет и очищаем очередь сообщений
    // Вызываем пользовательский колбэк
    net::post(strand_, [client_ptr]() {
        if (!client_ptr->is_connected_) {
            LOG_INFO("TcpClient is already disconnected");
            return;
        }
        boost::system::error_code ec;
        client_ptr->is_connected_ = false;
        client_ptr->socket_.close(ec);
        std::queue<std::vector<uint8_t>> empty;
        client_ptr->write_queue_.swap(empty);
        client_ptr->is_writing_ = false;
        LOG_INFO("TcpClient disconnected");
        // Вызоваем пользовательский колбэк
        if (client_ptr->disconnect_cb_) {
            client_ptr->disconnect_cb_();
        }
    });
}

void TcpClient::doRead() {
    auto client_ptr = shared_from_this();
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
                network::FrameCodec::decode(client_ptr->read_buffer_, [&client_ptr](network::PayloadView payload) {
                    if (client_ptr->message_cb_) {
                        std::vector<std::uint8_t> frame_data(payload.begin(), payload.end());
                        client_ptr->message_cb_(frame_data);
                    }
                });

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
    auto frame = network::FrameCodec::encode(std::move(payload));

    boost::asio::async_write(
        socket_,
        frame.buffers(),
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

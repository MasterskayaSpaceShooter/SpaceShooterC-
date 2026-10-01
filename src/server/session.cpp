#include "session.h"

#include <iostream>

#include "event_bus.h"
#include "frame_codec.h"
#include "logger.h"

Session::Session(boost::asio::ip::tcp::socket socket, SessionId id, EventBus& event_bus, FrameCodec& codec) :
    socket_(std::move(socket)), id_(id), event_bus_(event_bus), codec_(codec) {}

Session::~Session() {
    // LOG_INFO("Session destroyed");
}

void Session::start() {
    doRead();
}

/**
 * @brief Потокобезопасно ставит кадр в очередь отправки клиенту.
 * @details Взаимодействует с полями: write_queue_, write_mutex_, is_writing_.
 *          Запускает doWrite(), если в момент вызова отправка не идет.
 * @param data Входные данные: Массив байт отправляемого кадра.
 * @outputs Выходных значений нет.
 */
void Session::send(std::vector<uint8_t> data) {
    std::lock_guard lock(write_mutex_);
    write_queue_.push(data);

    if (!is_writing_) {
        doWrite();
    }
}

void Session::close() {
    socket_.close();
    // event_bus_.publish(ClientDisconnectedEvent{id_});
}

void Session::doRead() {
    if (!socket_.is_open()) {
        // LOG_ERR("Socket failed in session");
        return;
    }
    socket_.async_read_some(
        boost::asio::buffer(read_buffer_),
        [self = shared_from_this()](const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (error) {
                self->close();
                return;
            }

            auto mb = self->flat_buffer_.prepare(bytes_transferred);
            std::memcpy(mb.data(), self->read_buffer_.data(), bytes_transferred);
            self->flat_buffer_.commit(bytes_transferred);

            self->codec_.decode(self->flat_buffer_, [self](const std::vector<uint8_t>& payload) {
                self->event_bus_.publish(NetworkMessageEvent{self->id_, payload});
            });

            self->doRead();
        });
}

void Session::doWrite() {}

#include "session.h"

#include <iostream>

#include "event_bus.h"
#include "frame_codec.h"
// Заглушки для логирования
#ifndef LOG_INFO
#define LOG_INFO(msg) (void)0
#endif

#ifndef LOG_ERROR
#define LOG_ERROR(msg) (void)0
#endif

Session::Session(boost::asio::ip::tcp::socket socket,
                 SessionId id,
                 events::EventBus& event_bus,
                 network::FrameCodec& codec) :
    socket_(std::move(socket)), id_(id), event_bus_(event_bus), codec_(codec) {
    LOG_INFO("Session created");
}

Session::~Session() {
    close();
    LOG_INFO("Session destroyed");
}

void Session::start() {
    if (closed_.load()) {
        LOG_ERROR("Session closed");
        return;
    }
    LOG_INFO("Session started");
    doRead();
}

void Session::send(std::vector<uint8_t> data) {
    if (closed_.load()) {
        LOG_ERROR("Session closed");
        return;
    }
    bool should_start_write = false;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        write_queue_.push(std::move(data));

        if (!is_writing_) {
            is_writing_ = true;
            should_start_write = true;
        }
    }
    if (should_start_write) {
        doWrite();
    }
    LOG_INFO("Data in the queue");
}

void Session::close() {
    if (closed_.exchange(true)) {
        return;
    }
    boost::system::error_code ec;
    socket_.close(ec);
    LOG_INFO("Session closed");
    event_bus_.publish(ClientDisconnectedEvent{id_});
}

void Session::doRead() {
    if (closed_.load()) {
        return;
    }
    socket_.async_read_some(
        read_buffer_.prepare(READ_BLOCK_SIZE),
        [self = shared_from_this()](const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (error) {
                LOG_ERROR("Session read error");
                self->close();
                return;
            }

            self->read_buffer_.commit(bytes_transferred);

            network::FrameCodec::decode(self->read_buffer_, [self](network::PayloadView payload) {
                std::vector<uint8_t> frame_data(payload.begin(), payload.end());
                self->event_bus_.publish(NetworkMessageEvent{self->id_, frame_data});
            });

            self->doRead();
        });
}

void Session::doWrite() {
    if (closed_.load()) {
        return;
    }
    std::vector<uint8_t> payload;
    {
        std::lock_guard lock(write_mutex_);
        if (write_queue_.empty()) {
            is_writing_ = false;
            return;
        }
        payload = std::move(write_queue_.front());
        write_queue_.pop();
    }

    try {
        auto frame = network::FrameCodec::encode(std::move(payload));
        auto frame_ptr = std::make_shared<network::FrameCodec::Frame>(std::move(frame));

        boost::asio::async_write(
            socket_,
            frame_ptr->buffers(),
            [self = shared_from_this(), frame_ptr](const boost::system::error_code& ec, std::size_t) {
                if (ec) {
                    self->close();
                    return;
                }
                self->doWrite();
            });
    } catch (...) {
        LOG_ERROR("Session encode failed");
        close();
    }
}

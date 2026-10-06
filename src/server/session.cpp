#include "session.h"

#include "event_bus.h"
#include "frame_codec.h"

// TODO (a.shaporova): remove after valid logger implementation
#ifndef LOG_INFO
#define LOG_INFO(msg) (void)0
#endif

#ifndef LOG_ERROR
#define LOG_ERROR(msg) (void)0
#endif

Session::Session(boost::asio::ip::tcp::socket socket,
                 SessionId id,
                 std::shared_ptr<events::EventBus> event_bus,
                 network::FrameCodec& codec) :
    socket_(std::move(socket)), id_(id), event_bus_(std::move(event_bus)), codec_(codec),
    strand_(boost::asio::make_strand(socket_.get_executor())) {
    if (!event_bus_) {
        LOG_INFO(std::format("event_bus is null, id = {}", id_));
        throw std::invalid_argument("event_bus is null");
    }
    LOG_INFO(std::format("Session {} created", id_));
}

Session::~Session() {
    boost::system::error_code ec;
    if (socket_.is_open()) {
        socket_.close(ec);
    }
    LOG_INFO(std::format("Session {} destroyed", id_));
}

void Session::start() {
    if (closed_.load()) {
        LOG_INFO(std::format("Session {} closed", id_));
        return;
    }
    LOG_INFO(std::format("Session {} started", id_));
    boost::asio::post(strand_, [self = shared_from_this()] {
        self->doRead();
    });
}

void Session::send(const std::vector<uint8_t>& data) {
    boost::asio::post(strand_, [self = shared_from_this(), data = std::move(data)]() {
        if (self->closed_.load()) {
            LOG_ERROR(std::format("Session {} closed", self->id_));
            return;
        }

        self->write_queue_.push(std::move(data));
        if (!self->is_writing_) {
            self->is_writing_ = true;
            self->doWrite();
        }
        LOG_INFO(std::format("Session {} , data in the queue", self->id_));
    });
}

void Session::close() {
    if (closed_.exchange(true)) {
        return;
    }

    boost::asio::post(strand_, [self = shared_from_this()] {
        boost::system::error_code ec;
        self->socket_.close(ec);
        LOG_INFO(std::format("Session {} closed", self->id_));
        self->event_bus_->publish(ClientDisconnectedEvent{self->id_});
    });
}

void Session::doRead() {
    if (closed_.load()) {
        return;
    }
    socket_.async_read_some(
        read_buffer_.prepare(READ_BLOCK_SIZE),
        boost::asio::bind_executor(
            strand_,
            [self = shared_from_this()](const boost::system::error_code& error, std::size_t bytes_transferred) {
                if (error) {
                    if (error != boost::asio::error::operation_aborted) {
                        LOG_ERROR(std::format("Session {} read error: {}", self->id_, error.message()));
                    }
                    self->close();
                    return;
                }

                self->read_buffer_.commit(bytes_transferred);

                auto messages = network::FrameCodec::decode(self->read_buffer_, [self](network::PayloadView payload) {
                    std::vector<uint8_t> frame_data(payload.begin(), payload.end());
                    self->event_bus_->publish(NetworkMessageEvent{self->id_, frame_data});
                });

                if (messages > 0) {
                    LOG_INFO(std::format("Session {} decoded frames", self->id_));
                }
                self->doRead();
            }));
}

void Session::doWrite() {
    if (closed_.load() || write_queue_.empty()) {
        is_writing_ = false;
        return;
    }
    std::vector<uint8_t> payload;

    payload = std::move(write_queue_.front());
    write_queue_.pop();

    try {
        auto frame = network::FrameCodec::encode(std::move(payload));
        auto frame_ptr = std::make_shared<network::FrameCodec::Frame>(std::move(frame));

        boost::asio::async_write(
            socket_,
            frame_ptr->buffers(),
            boost::asio::bind_executor(
                strand_,
                [self = shared_from_this(), frame_ptr](const boost::system::error_code& ec, std::size_t) {
                    if (ec) {
                        if (ec != boost::asio::error::operation_aborted) {
                            LOG_ERROR(std::format("Session {} write error: {}", self->id_, ec.message()));
                        }
                        self->is_writing_ = false;
                        self->close();
                        return;
                    }
                    self->doWrite();
                }));
    } catch (...) {
        LOG_ERROR(std::format("Session {} encode failed", id_));
        is_writing_ = false;
        close();
    }
}

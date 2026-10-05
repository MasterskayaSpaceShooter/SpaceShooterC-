#include "tcp_server.h"

#include <stdexcept>

#include "frame_codec.h"
#include "logger.h"
#include "network_events.h"
#include "session.h"

namespace {

std::shared_ptr<events::EventBus> requireEventBus(std::shared_ptr<events::EventBus> event_bus) {
    if (!event_bus) {
        throw std::invalid_argument("event_bus must not be null");
    }

    return event_bus;
}

}  // namespace

TcpServer::TcpServer(boost::asio::io_context& io_context,
                     uint16_t port,
                     std::shared_ptr<events::EventBus> event_bus,
                     network::FrameCodec& codec) :
    acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
    event_bus_(requireEventBus(std::move(event_bus))), codec_(codec), session_registry_(event_bus_) {}

void TcpServer::start() {
    boost::system::error_code ec;
    const auto endpoint = acceptor_.local_endpoint(ec);

    if (!ec) {
        LOG_INFO("TCP server started on port {}", endpoint.port());
    } else {
        LOG_WARN("Failed to retrieve local endpoint: {}", ec.message());
    }

    doAccept();
}

void TcpServer::stop() {
    boost::system::error_code error_code;
    acceptor_.close(error_code);

    if (error_code) {
        LOG_ERROR("Failed to stop TCP server: {}", error_code.message());
        return;
    }

    LOG_INFO("TCP server stopped");
}

void TcpServer::doAccept() {
    auto self = shared_from_this();

    acceptor_.async_accept([self](boost::system::error_code error_code, boost::asio::ip::tcp::socket socket) {
        if (error_code) {
            if (error_code == boost::asio::error::operation_aborted) {
                return;
            }

            LOG_ERROR("Failed to accept client: {}", error_code.message());

            if (self->acceptor_.is_open()) {
                self->doAccept();
            }

            return;
        }

        const SessionId session_id = self->next_session_id_.fetch_add(1, std::memory_order_relaxed);

        boost::system::error_code endpoint_error;
        const auto endpoint = socket.remote_endpoint(endpoint_error);

        std::string remote_address;

        if (!endpoint_error) {
            remote_address = endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
        } else {
            LOG_WARN("Failed to get remote endpoint for session {}: {}", session_id, endpoint_error.message());
        }

        auto session = std::make_shared<Session>(std::move(socket), session_id, self->event_bus_, self->codec_);

        self->session_registry_.addSession(session);

        session->start();

        self->event_bus_->publish(ClientConnectedEvent(session_id, remote_address));

        LOG_INFO("Client connected: session_id={}, address={}", session_id, remote_address);

        if (self->acceptor_.is_open()) {
            self->doAccept();
        }
    });
}

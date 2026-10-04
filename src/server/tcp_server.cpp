#include "tcp_server.h"

#include "frame_codec.h"
#include "logger.h"
#include "network_events.h"
#include "session.h"

TcpServer::TcpServer(boost::asio::io_context& io_context,
                     uint16_t port,
                     std::shared_ptr<events::EventBus> event_bus,
                     network::FrameCodec& codec) :
    acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
    event_bus_(std::move(event_bus)), codec_(codec), session_registry_(event_bus_) {}

void TcpServer::start() {
    doAccept();
    LOG_INFO("TCP server started on port {}", acceptor_.local_endpoint().port());
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
            return;
        }

        const SessionId session_id = self->next_session_id_.fetch_add(1);

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

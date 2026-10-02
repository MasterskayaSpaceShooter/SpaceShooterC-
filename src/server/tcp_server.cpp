#include "tcp_server.h"

#include "network_events.h"
#include "session.h"

TcpServer::TcpServer(boost::asio::io_context& io_context, uint16_t port, events::EventBus& event_bus, network::FrameCodec& codec) :
    acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)), event_bus_(event_bus),
    codec_(codec), session_registry_(event_bus) {}

void TcpServer::start() {
    doAccept();
}

void TcpServer::stop() {
    boost::system::error_code error_code;
    acceptor_.close(error_code);
}

void TcpServer::doAccept() {
    acceptor_.async_accept([this](boost::system::error_code error_code, boost::asio::ip::tcp::socket socket) {
        if (!error_code) {
            const SessionId session_id = next_session_id_.fetch_add(1);

            boost::system::error_code endpoint_error;
            const auto endpoint = socket.remote_endpoint(endpoint_error);

            std::string remote_address;

            if (!endpoint_error) {
                remote_address = endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
            }

            auto session = std::make_shared<Session>(std::move(socket), session_id, event_bus_, codec_);

            session_registry_.addSession(session);

            session->start();

            event_bus_.publish(ClientConnectedEvent(session_id, remote_address));
        }

        if (acceptor_.is_open()) {
            doAccept();
        }
    });
}

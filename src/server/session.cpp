#include "session.h"

Session::Session(boost::asio::ip::tcp::socket socket, SessionId id, EventBus& event_bus, FrameCodec& codec) :
    socket_(std::move(socket)), id_(id), event_bus_(event_bus), codec_(codec) {}

Session::~Session() {}

void Session::start() {}

void Session::send(std::vector<uint8_t>) {}

void Session::close() {}

void Session::doRead() {}

void Session::doWrite() {}

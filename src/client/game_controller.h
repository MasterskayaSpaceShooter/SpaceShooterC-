#pragma once

#include <atomic>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "client_state.h"
#include "tcp_client.h"

namespace client {

/**
 * @brief MVP controller for the game client.
 *
 * @par Scope
 * Owns the client state machine and issues commands to the server.
 * For now the only command is JOIN; the payload is a plain ASCII string
 * `"JOIN <username>"`.
 *
 * @par Threading model
 * Async. All mutation happens on `strand_`. Public methods are callable from
 * any thread: they `post` work onto the strand and return immediately.
 * `state()` is a lock-free atomic load, safe to read from any thread.
 *
 * Two strands are in play:
 *  - TcpClient::strand_          — where TcpClient invokes its callbacks.
 *  - GameController::strand_     — where GameController mutates its state.
 * A message arriving from the socket hops TcpClient::strand_ ->
 * GameController::strand_ so that socket-driven writes and UI/stdin-driven
 * writes share one queue.
 *
 * @par Lifetime
 * Derives from std::enable_shared_from_this. Must be owned by a shared_ptr.
 * All callbacks capture `weak_from_this()`; if the controller is gone, the
 * callback silently no-ops.
 *
 * @par Re-entrancy
 * start() and shutdown() may be called concurrently from any threads.
 * Ordering between them is the order their posts land on strand_. The
 * connect callback re-checks state_ == Connecting after its hop to the
 * strand, so a shutdown() that raced with an in-flight connect is
 * honored.
 *
 * This class does NOT call shutdown() in its destructor. The program is
 * required to shut down cleanly, in this order:
 *   1. call shutdown()
 *   2. let io.run() / io.poll() drain pending handlers
 *   3. drop the last shared_ptr<GameController>
 *   4. destroy io_context
 *
 * @warning `weak_from_this()` protects callbacks only against
 *          GameController being destroyed. It does NOT protect against
 *          TcpClient or io_context dying first. Both must outlive this
 *          controller. Destroying either before step 3 above is UB.
 *
 * @par Construction
 * Protected ctor. Callers must use GameController::create().
 */
class GameController : public std::enable_shared_from_this<GameController> {
public:
    using TcpClientPtr = std::shared_ptr<TcpClient>;

    /**
     * @brief Factory. Must be used instead of the constructor so that
     *        `weak_from_this()` is populated before callbacks are wired.
     *
     * @param tcp Transport. Must outlive this controller.
     * @param io  io_context used to create the controller's strand.
     *            Must outlive this controller.
     */
    [[nodiscard]] static std::shared_ptr<GameController> create(TcpClientPtr tcp, boost::asio::io_context& io);

    GameController(const GameController&) = delete;
    GameController& operator=(const GameController&) = delete;

    /**
     * @brief Connect and send JOIN on success.
     *
     * @note Posted unconditionally. The re-entry guard lives in doStart() so
     * concurrent start() / shutdown() calls are serialized on the strand.
     */
    void start(std::string host, std::uint16_t port, std::string playerName);

    /**
     * @brief Idempotent. Calling shutdown() when already Disconnected posts a
     * handler that disconnects an already-disconnected socket — a no-op at
     * the TcpClient level.
     */
    void shutdown();

    /// @brief Lock-free snapshot of the client state. Safe from any thread.
    [[nodiscard]] ClientState state() const noexcept;

protected:
    GameController(TcpClientPtr tcp, boost::asio::io_context& io) :
        tcp_(std::move(tcp)), strand_(boost::asio::make_strand(io)) {}

private:
    /**
     * @brief Wires TcpClient callbacks. Called once from create(), after
     *        weak_from_this() is guaranteed to be populated.
     *
     * @par Strand hop
     * TcpClient invokes these on TcpClient::strand_. Immediately re-posted
     * to GameController::strand_ so all state mutation shares one queue.
     */
    void wireCallbacks();

    void doStart(const std::string& host, std::uint16_t port, std::string playerName);

    void doShutdown();

    /**
     * @brief Sends the JOIN command.
     *
     * @par MVP wire format
     * @code
     *   payload = "JOIN <playerName>"     // raw ASCII, no opcode, no length
     * @endcode
     *
     * @warning Temporary format.
     */
    void sendJoin();

    /**
     * @brief Handles a server message. MVP string protocol.
     *
     * Recognised prefixes:
     *   - "ACCEPT"  -> InGame
     *   - "REJECT"  -> Disconnected
     *
     * @warning Temporary string dispatch. Same note as sendJoin().
     */
    void onMessageOnStrand(const std::vector<std::uint8_t>& payload);

    /**
     * @brief Updates `state_`. No-op if unchanged. Strand-only.
     */
    void setState(ClientState next) noexcept;

    TcpClientPtr tcp_;

    /// Serializes all GameController state mutation.
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;

    /// Atomic because state() is a lock-free getter callable from any thread.
    std::atomic<ClientState> state_{ClientState::Disconnected};

    /// Touched only from strand_ (doStart writes, sendJoin reads).
    std::string playerName_;
};

}  // namespace client

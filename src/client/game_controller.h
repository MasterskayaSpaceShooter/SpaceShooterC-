#pragma once

#include <atomic>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "client_state.h"
#include "client_world.h"
#include "tcp_client.h"

namespace client {

/**
 * @brief MVP controller for the game client.
 *
 * @par Scope
 * Owns the client state machine and issues commands to the server.
 * For now the only command is JOIN; the wire payload is `"JOIN <name>"`
 * with a single space as delimiter. playerName containing invalid characters
 * (whitespace / control / NUL) — rejected.
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
 * start(), shutdown(), and rejoin() may be called concurrently from any
 * threads. Ordering between them is the order their posts land on strand_.
 * The connect callback re-checks state_ == Connecting AND matches the
 * connectGen_ it was issued with after its hop to the strand, so a
 * shutdown() or rejoin() that raced with an in-flight connect is honored
 * (stale callback dropped).
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
     * @note playerName is validated synchronously on the calling thread
     * before posting. If invalid, LOG_ERROR is emitted and the function
     * returns WITHOUT posting and WITHOUT changing state.
     *
     * If playerName is valid, the work is posted to the strand. The
     * re-entry guard lives in doStart() so concurrent start() / shutdown()
     * are serialized on the strand. doStart() only proceeds when the
     * current state is Disconnected; from any other state (e.g. GameEnd)
     * this call is a logged no-op. To reset from an arbitrary state, use
     * rejoin().
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

    /**
     * @brief Ends the current session and starts a fresh one.
     *
     * @note playerName is validated synchronously on the calling thread
     * before posting. If invalid, LOG_ERROR is emitted and the function
     * returns WITHOUT ending the current session: state is left
     * unchanged (e.g. stays in GameEnd / InGame).
     *
     * If playerName is valid, posts doShutdown() and doStart()
     * back-to-back on strand_, so the two cannot be interleaved with a
     * racing start() / shutdown() / rejoin().
     *
     * @par Rejoin policy (MVP: zero cooldown)
     * There is NO client-side cooldown. After GAME_END the app may call
     * rejoin() immediately. The server is the sole authority on whether a
     * rejoin is premature and signals that with REJECT.
     *
     * @par Session model
     * Every rejoin is a fresh TCP session + fresh JOIN. The controller never
     * attempts to reuse an already-open socket after GAME_END. This makes state
     * unambiguous: either the new connect succeeds and JOIN is sent, or it
     * fails and the client is Disconnected.
     */
    void rejoin(std::string host, std::uint16_t port, std::string playerName);

    void shoot();

protected:
    GameController(TcpClientPtr tcp, boost::asio::io_context& io) :
        tcp_(std::move(tcp)), strand_(boost::asio::make_strand(io)) {}

private:
    /**
     * @brief Wires TcpClient callbacks. Called once from create().
     *
     * @par Lifetime invariant
     * Never unwired. doShutdown() does NOT clear callbacks, so a subsequent
     * start() / rejoin() reuses the same wiring without a cross-strand
     * reinstall race. Two guards make this safe:
     *
     *  - onMessageOnStrand() drops incoming messages while Disconnected
     *    (late delivery from a prior session, or after the server closed
     *    the socket following GAME_END).
     *  - disconnect handler drops events while Disconnected (redundant)
     *    or Connecting (stale: the disconnect posted by rejoin()'s
     *    doShutdown() can fire after the new connect has already started).
     */
    void wireCallbacks();

    void doStart(const std::string& host, std::uint16_t port, std::string playerName);

    void doShutdown();

    /**
     * @brief Sends the JOIN command via protocol::encode.
     */
    void sendJoin();

    /**
     * @brief Handles a server message by delegating to protocol::decode().
     *
     * Dispatch table:
     *   - Accept  -> InGame
     *   - Reject  -> doShutdown() (Disconnected; server refused the session)
     *   - GameEnd -> GameEnd       (match over for THIS client)
     *   - Unknown -> logged, state unchanged
     *
     * Messages arriving while Disconnected are dropped (late delivery after
     * shutdown, or after the server closed the socket following GAME_END).
     */
    void onMessageOnStrand(const std::vector<std::uint8_t>& payload);

    /**
     * @brief Updates `state_`. No-op if unchanged. Strand-only.
     */
    void setState(ClientState next) noexcept;

    /**
     * @brief Validates playerName against the wire-protocol requirements
     *        of "JOIN <name>": non-empty, and no character <= 0x20 or
     *        0x7f. Emits LOG_ERROR on failure (side effect). Does not
     *        touch state. Called synchronously from start()/rejoin()
     *        before the strand post.
     */
    static bool validatePlayerName(const std::string& name);

    void doShoot();

    TcpClientPtr tcp_;

    /// Serializes all GameController state mutation.
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;

    /// Atomic because state() is a lock-free getter callable from any thread.
    std::atomic<ClientState> state_{ClientState::Disconnected};

    /// Strand-only. Bumped on every connect() call. Each connect callback
    /// captures the generation it was issued with; on strand, a mismatch
    /// means the callback belongs to a superseded connect attempt and is
    /// dropped. Prevents a stale in-flight connect from winning against a
    /// newer one after rejoin()/shutdown()+start().
    std::uint64_t connectGen_ = 0;

    /// Touched only from strand_ (doStart writes, sendJoin reads).
    std::string playerName_;

    ClientWorld world_;

    std::chrono::steady_clock::time_point last_shot_time_{std::chrono::steady_clock::time_point::min()};
    static constexpr auto kShootCooldown = std::chrono::milliseconds(50);
};

}  // namespace client

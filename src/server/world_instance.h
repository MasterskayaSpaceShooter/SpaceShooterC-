#pragma once

#include <boost/asio.hpp>
#include <boost/signals2.hpp>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "entity.h"
#include "entity_pool.h"
#include "entity_snapshot.h"
#include "event_bus.h"
#include "game_snapshot.h"
#include "math_utils.h"
#include "network_events.h"
#include "thread_safe_queue.h"
#include "types.h"

namespace domain {

namespace world {
inline constexpr math::Scalar kDefaultWidth = 2000.0;
inline constexpr math::Scalar kDefaultHeight = 2000.0;
inline constexpr math::Scalar kShipDefaultMass = 1.0;
inline constexpr math::Scalar kShipDefaultRadius = 20.0;
inline constexpr math::Scalar kShipDefaultThrustPower = 400.0;
inline constexpr math::Scalar kShipDefaultDrag = 1.2;
inline constexpr std::uint16_t kShipDefaultHealth = 100;
inline constexpr std::uint16_t kShipDefaultDamage = 25;
inline constexpr std::size_t kMaxLeaderboardSize = 10;
inline constexpr std::size_t kMaxMissedTicks = 5;
inline constexpr math::Scalar kBulletRadius = 3.0;
inline constexpr math::Scalar kBulletSpeed = 650.0;
}  // namespace world

class WorldInstance : public std::enable_shared_from_this<WorldInstance> {
public:
    enum class State {
        Stopped,
        Running
    };

    enum class ActionType {
        PlayerJoin,
        PlayerQuit,
        PlayerMove,
        PlayerShoot,
        PlayerRespawn
    };

    struct ActionEvent {
        ActionType type{ActionType::PlayerJoin};
        SessionId session_id{0};
        std::string player_name;
        math::Scalar angle{0.0};
    };

    struct PendingJoin {
        SessionId session_id{0};
        std::string player_name;
    };

    struct CreateToken {
        explicit CreateToken() = default;
    };

    static std::shared_ptr<WorldInstance> create(boost::asio::io_context& io_context,
                                                 std::shared_ptr<events::EventBus> event_bus,
                                                 double tick_rate_hz = 60.0,
                                                 math::Scalar world_width = world::kDefaultWidth,
                                                 math::Scalar world_height = world::kDefaultHeight);

    WorldInstance(boost::asio::io_context& io_context,
                  std::shared_ptr<events::EventBus> event_bus,
                  double tick_rate_hz,
                  CreateToken,
                  math::Scalar world_width,
                  math::Scalar world_height);

    ~WorldInstance();

    WorldInstance(const WorldInstance&) = delete;
    WorldInstance& operator=(const WorldInstance&) = delete;
    WorldInstance(WorldInstance&&) = delete;
    WorldInstance& operator=(WorldInstance&&) = delete;

    void start();
    void stop();
    void enqueueAction(ActionEvent action);

    [[nodiscard]] State state() const noexcept { return state_.load(); }
    [[nodiscard]] std::uint64_t currentTick() const noexcept;
    [[nodiscard]] std::shared_ptr<const snapshot::GameSnapshot> lastSnapshot() const;
    [[nodiscard]] std::chrono::microseconds tickInterval() const noexcept { return tick_interval_; }
    [[nodiscard]] std::pair<math::Scalar, math::Scalar> worldBounds() const noexcept {
        return {world_width_, world_height_};
    }

    [[nodiscard]] std::vector<math::Vector2D> freePositions(std::size_t count,
                                                            math::Scalar clearance = 25.0) const;
    [[nodiscard]] math::Circle freePosition() const;
    [[nodiscard]] std::vector<snapshot::LeaderboardEntry> buildLeaderboard() const;

    EntityPool& entities() noexcept { return entities_; }
    const EntityPool& entities() const noexcept { return entities_; }
    std::shared_mutex& entitiesMutex() const noexcept { return entities_mutex_; }

    void resolveCollisions();

    // Друзья для тестов
    friend class WorldInstanceTestAccessor;
    friend class TwoShipsShootTestAccessor;

public:
    void doStart();
    void doStop();
    void shutdownTickLoop();
    void scheduleNextTick();
    void doTick(const boost::system::error_code& error);
    void runTickStep();

    void processInputQueue(std::vector<PendingJoin>& new_players);
    void updatePhysics(math::Scalar dt);
    void maintainAsteroidPopulation();
    void spawnShips(const std::vector<PendingJoin>& new_players);
    void spawnBullet(const domain::Entity& shooter);

    [[nodiscard]] bool mayCollide(types::EntityType a, types::EntityType b) const noexcept;
    void dispatchCollision(domain::Entity& a, domain::Entity& b);

    void onCollisionShipShip(domain::Entity& ship1, domain::Entity& ship2);
    void onCollisionShipAsteroid(domain::Entity& ship, domain::Entity& asteroid);
    void onCollisionBulletShip(domain::Entity& proj, domain::Entity& ship);
    void onCollisionBulletAsteroid(domain::Entity& proj, domain::Entity& asteroid);
    void reapDestroyedShips();

    snapshot::GameSnapshot buildGameSnapshot();
    void broadcastSnapshot(const snapshot::GameSnapshot& snapshot);

    void setupSubscriptions();
    void onClientDisconnected(const ClientDisconnectedEvent& event);
    void onPlayerJoin(const PlayerJoinEvent& event);
    void onPlayerMove(const PlayerMoveEvent& event);
    void onPlayerShoot(const PlayerShootEvent& event);
    void onPlayerRespawn(const PlayerRespawnEvent& event);

public:
    std::shared_ptr<events::EventBus> event_bus_;
    boost::asio::strand<boost::asio::io_context::executor_type> tick_strand_;
    boost::asio::steady_timer tick_timer_;
    std::chrono::microseconds tick_interval_{0};

    math::Scalar world_width_{world::kDefaultWidth};
    math::Scalar world_height_{world::kDefaultHeight};

    std::atomic<State> state_{State::Stopped};
    std::atomic<std::uint64_t> current_tick_{0};

    mutable std::shared_mutex entities_mutex_;
    EntityPool entities_;
    EntityPool& entities_ref{entities_};

    mutable std::mutex action_mutex_;
    ThreadSafeQueue<ActionEvent> action_queue_;

    mutable std::mutex last_snapshot_mutex_;
    std::shared_ptr<const snapshot::GameSnapshot> last_snapshot_;

    std::unordered_map<SessionId, types::EntityId> session_to_ship_;
    std::vector<snapshot::EntitySnapshot> snapshot_buffer_;

    boost::signals2::scoped_connection sub_disconnect_;
    boost::signals2::scoped_connection sub_player_join_;
    boost::signals2::scoped_connection sub_player_move_;
    boost::signals2::scoped_connection sub_player_shoot_;
    boost::signals2::scoped_connection sub_player_respawn_;
};

}  // namespace domain

// Предоставляем WorldInstance и ActionEvent в глобальном пространстве имён для тестов
using WorldInstance = domain::WorldInstance;
using ActionEvent = domain::WorldInstance::ActionEvent;

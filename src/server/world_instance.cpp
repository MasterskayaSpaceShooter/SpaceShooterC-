#include "world_instance.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "collision_utils.h"
#include "entity_utils.h"
#include "logger.h"
#include "math_utils.h"
#include "network_events.h"
#include "network_response_router.h"
#include "asteroid_factory.h"

namespace domain {

std::shared_ptr<WorldInstance> WorldInstance::create(boost::asio::io_context& io_context,
                                                     std::shared_ptr<events::EventBus> event_bus,
                                                     double tick_rate_hz,
                                                     math::Scalar world_width,
                                                     math::Scalar world_height) {
    auto world = std::make_shared<WorldInstance>(io_context,
                                                 std::move(event_bus),
                                                 tick_rate_hz,
                                                 CreateToken{},
                                                 world_width,
                                                 world_height);
    world->setupSubscriptions();
    return world;
}

WorldInstance::WorldInstance(boost::asio::io_context& io_context,
                             std::shared_ptr<events::EventBus> event_bus,
                             double tick_rate_hz,
                             CreateToken,
                             math::Scalar world_width,
                             math::Scalar world_height)
    : event_bus_(std::move(event_bus)),
      tick_strand_(boost::asio::make_strand(io_context)),
      tick_timer_(io_context),
      tick_interval_{},
      world_width_(world_width),
      world_height_(world_height) {
    if (!event_bus_) {
        LOG_ERROR("[WorldInstance] event_bus is nullptr");
        throw std::invalid_argument("WorldInstance: event_bus must not be nullptr");
    }
    if (tick_rate_hz <= 0.0) {
        LOG_ERROR("[WorldInstance] invalid tick_rate_hz={}", tick_rate_hz);
        throw std::invalid_argument("WorldInstance: tick_rate_hz must be positive");
    }
    if (world_width_ <= 0.0 || world_height_ <= 0.0) {
        LOG_ERROR("[WorldInstance] invalid world_size: {}x{}", world_width_, world_height_);
        throw std::invalid_argument("WorldInstance: world_width and world_height must be positive");
    }

    tick_interval_ =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<double>(1.0 / tick_rate_hz));
    if (tick_interval_.count() < 1) {
        tick_interval_ = std::chrono::microseconds(1);
    }
}

WorldInstance::~WorldInstance() {
    boost::system::error_code cancel_error;
    tick_timer_.cancel(cancel_error);
}

void WorldInstance::start() {
    boost::asio::post(tick_strand_, [self = shared_from_this()] {
        self->doStart();
    });
}

void WorldInstance::stop() {
    boost::asio::post(tick_strand_, [self = shared_from_this()] {
        self->doStop();
    });
}

void WorldInstance::doStart() {
    try {
        State expected{State::Stopped};
        if (!state_.compare_exchange_strong(expected, State::Running)) {
            return;
        }

        LOG_INFO("[WorldInstance] start: tick_interval={} ms",
                 std::chrono::duration<double, std::milli>(tick_interval_).count());

        tick_timer_.expires_after(tick_interval_);
        tick_timer_.async_wait(
            boost::asio::bind_executor(tick_strand_, [self = shared_from_this()](const boost::system::error_code& ec) {
                self->doTick(ec);
            }));
    } catch (const std::exception& e) {
        LOG_ERROR("[WorldInstance] start error: {}", e.what());
        state_.store(State::Stopped);
    } catch (...) {
        LOG_ERROR("[WorldInstance] start unknown error");
        state_.store(State::Stopped);
    }
}

void WorldInstance::doStop() {
    try {
        State expected{State::Running};
        if (!state_.compare_exchange_strong(expected, State::Stopped)) {
            return;
        }

        LOG_INFO("[WorldInstance] stop");
        shutdownTickLoop();
    } catch (const std::exception& e) {
        LOG_ERROR("[WorldInstance] stop error: {}", e.what());
    } catch (...) {
        LOG_ERROR("[WorldInstance] stop unknown error");
    }
}

void WorldInstance::shutdownTickLoop() {
    boost::system::error_code cancel_error;
    tick_timer_.cancel(cancel_error);
    if (cancel_error) {
        LOG_ERROR("[WorldInstance] stop: timer cancel failed: {}", cancel_error.message());
    }

    std::lock_guard action_lock(action_mutex_);
    const auto discarded = action_queue_.drain();
    if (!discarded.empty()) {
        LOG_WARN("[WorldInstance] stop: discarded {} queued actions", discarded.size());
    }
}

void WorldInstance::enqueueAction(ActionEvent action) {
    std::lock_guard lock(action_mutex_);
    if (state_.load() != State::Running) {
        LOG_WARN("[WorldInstance] discarded action because the world is stopped");
        return;
    }
    action_queue_.push(std::move(action));
}

void WorldInstance::doTick(const boost::system::error_code& error) {
    try {
        if (error) {
            if (error == boost::asio::error::operation_aborted) {
                return;
            }
            LOG_ERROR("[WorldInstance] tick timer error: {}", error.message());
            state_.store(State::Stopped);
            shutdownTickLoop();
            return;
        }

        if (state_.load() != State::Running) {
            return;
        }

        scheduleNextTick();

        runTickStep();
        ++current_tick_;
    } catch (const std::exception& e) {
        LOG_ERROR("[WorldInstance] tick #{} error: {}", current_tick_.load(), e.what());
        state_.store(State::Stopped);
        shutdownTickLoop();
    } catch (...) {
        LOG_ERROR("[WorldInstance] tick #{} unknown error", current_tick_.load());
        state_.store(State::Stopped);
        shutdownTickLoop();
    }
}

void WorldInstance::runTickStep() {
    std::unique_lock entities_lock(entities_mutex_);
    try {
        std::vector<PendingJoin> new_players;
        processInputQueue(new_players);
        updatePhysics(static_cast<math::Scalar>(tick_interval_.count()) / 1'000'000.0);
        resolveCollisions();
        maintainAsteroidPopulation();
        spawnShips(new_players);

        auto shared_snapshot = std::make_shared<const snapshot::GameSnapshot>(buildGameSnapshot());
        {
            std::lock_guard lock(last_snapshot_mutex_);
            last_snapshot_ = shared_snapshot;
        }
        broadcastSnapshot(*shared_snapshot);
        entities_.recycleDestroyed();
    } catch (const std::exception& e) {
        LOG_ERROR("[WorldInstance] tick #{} error: {}", current_tick_.load(), e.what());
    } catch (...) {
        LOG_ERROR("[WorldInstance] tick #{} unknown error", current_tick_.load());
    }
}

void WorldInstance::scheduleNextTick() {
    if (state_.load() != State::Running) {
        return;
    }

    const auto next_deadline = tick_timer_.expiry() + tick_interval_;

    if (next_deadline < std::chrono::steady_clock::now() - tick_interval_ * world::kMaxMissedTicks) {
        tick_timer_.expires_after(tick_interval_);
    } else {
        tick_timer_.expires_at(next_deadline);
    }

    tick_timer_.async_wait(
        boost::asio::bind_executor(tick_strand_, [self = shared_from_this()](const boost::system::error_code& ec) {
            self->doTick(ec);
        }));
}

std::uint64_t WorldInstance::currentTick() const noexcept {
    return current_tick_.load();
}

std::shared_ptr<const snapshot::GameSnapshot> WorldInstance::lastSnapshot() const {
    std::lock_guard lock(last_snapshot_mutex_);
    return last_snapshot_;
}

std::vector<math::Vector2D> WorldInstance::freePositions(std::size_t count, math::Scalar clearance) const {
    if (count == 0) {
        return {};
    }
    if (!std::isfinite(clearance) || clearance < 0.0) {
        throw std::invalid_argument("WorldInstance: clearance must be finite and non-negative");
    }

    const auto half_width = world_width_ * 0.5;
    const auto half_height = world_height_ * 0.5;
    if (clearance > half_width || clearance > half_height) {
        return {};
    }

    std::vector<math::Vector2D> positions;
    const auto spacing = std::max(2.0 * clearance, 1.0);
    const auto min_x = -half_width + clearance;
    const auto max_x = half_width - clearance;
    const auto min_y = -half_height + clearance;
    const auto max_y = half_height - clearance;

    for (auto y = min_y; y <= max_y && positions.size() < count; y += spacing) {
        for (auto x = min_x; x <= max_x && positions.size() < count; x += spacing) {
            const math::Vector2D candidate(x, y);
            const auto overlaps = std::any_of(entities_.storage().begin(),
                                              entities_.storage().end(),
                                              [&candidate, clearance](const auto& slot) {
                                                  if (!slot || slot->isDestroyed || !slot->transform) {
                                                      return false;
                                                  }
                                                  const auto combined_radius = clearance + slot->radius;
                                                  return (candidate - slot->transform->position).lengthSquared() <=
                                                         combined_radius * combined_radius;
                                              });
            if (!overlaps) {
                positions.push_back(candidate);
            }
        }
    }
    return positions;
}

math::Circle WorldInstance::freePosition() const {
    std::vector<std::pair<math::Vector2D, math::Scalar>> occupied;
    occupied.reserve(entities_.storage().size());
    for (const auto& slot : entities_.storage()) {
        if (slot.has_value() && !slot->isDestroyed && slot->transform.has_value()) {
            occupied.emplace_back(slot->transform->position, slot->radius);
        }
    }
    return math::largestEmptyCircle(occupied, world_width_, world_height_);
}

void WorldInstance::processInputQueue(std::vector<PendingJoin>& new_players) {
    const auto queued_actions = action_queue_.drain();
    for (const auto& action : queued_actions) {
        switch (action.type) {
            case ActionType::PlayerJoin: {
                if (session_to_ship_.contains(action.session_id)) {
                    LOG_WARN("[WorldInstance] Duplicate PlayerJoin for session {}, ignoring", action.session_id);
                    break;
                }
                const bool already_pending = std::any_of(
                    new_players.begin(), new_players.end(),
                    [&](const PendingJoin& p) { return p.session_id == action.session_id; });
                if (!already_pending) {
                    new_players.push_back(PendingJoin{action.session_id, action.player_name});
                }
                break;
            }
            case ActionType::PlayerRespawn: {
                if (!session_to_ship_.contains(action.session_id)) {
                    const bool already_pending = std::any_of(
                        new_players.begin(), new_players.end(),
                        [&](const PendingJoin& p) { return p.session_id == action.session_id; });
                    if (!already_pending) {
                        new_players.push_back(PendingJoin{action.session_id, action.player_name.empty() ? std::string("Pilot") : action.player_name});
                        LOG_INFO("[WorldInstance] Session {} queued for RESPAWN", action.session_id);
                    }
                }
                break;
            }
            case ActionType::PlayerQuit: {
                auto it = session_to_ship_.find(action.session_id);
                if (it == session_to_ship_.end())
                    break;

                const types::EntityId ship_id = it->second;
                for (auto& entity : entities_.storage()) {
                    if (entity && entity->id == ship_id) {
                        entity->isDestroyed = true;
                        break;
                    }
                }
                session_to_ship_.erase(it);
                break;
            }
            case ActionType::PlayerMove: {
                auto it = session_to_ship_.find(action.session_id);
                if (it == session_to_ship_.end())
                    break;

                auto* ship = entities_.findByEntityID(it->second);
                if (!ship || ship->isDestroyed || !ship->transform)
                    break;

                ship->transform->angle = action.angle;
                // Применяем тягу напрямую в velocity по углу
                const math::Vector2D forward(std::cos(action.angle), std::sin(action.angle));
                ship->transform->velocity += forward * 20.0;
                break;
            }
            case ActionType::PlayerShoot: {
                auto it = session_to_ship_.find(action.session_id);
                if (it == session_to_ship_.end())
                    break;

                auto* ship = entities_.findByEntityID(it->second);
                if (!ship || ship->isDestroyed || !ship->transform)
                    break;

                spawnBullet(*ship);
                break;
            }
        }
    }
}

void WorldInstance::spawnBullet(const domain::Entity& shooter) {
    if (!shooter.transform) {
        return;
    }

    auto* bullet = entities_.acquire();
    bullet->type = types::EntityType::Bullet;
    bullet->mass = 0.05;
    bullet->radius = world::kBulletRadius;

    const math::Scalar angle = shooter.transform->angle;
    const math::Vector2D forward(std::cos(angle), std::sin(angle));
    const math::Vector2D spawn_pos =
        shooter.transform->position + forward * (shooter.radius + bullet->radius + 4.0);

    bullet->transform = domain::TransformComponent{
        spawn_pos,
        forward * world::kBulletSpeed,
        angle
    };
    bullet->combat = domain::CombatComponent{world::kShipDefaultDamage, shooter.id};
    bullet->health = domain::HealthComponent{1, 1};
}

void WorldInstance::updatePhysics(math::Scalar dt) {
    const auto half_w = world_width_ * static_cast<math::Scalar>(0.5);
    const auto half_h = world_height_ * static_cast<math::Scalar>(0.5);

    for (auto& slot : entities_.storage()) {
        if (!entities_.isActive(slot) || !slot->transform) {
            continue;
        }

        auto& e = *slot;
        auto& t = *e.transform;

        if (e.type == types::EntityType::Ship) {
            t.velocity *= std::clamp(1.0 - (world::kShipDefaultDrag * dt), 0.0, 1.0);
            t.position += t.velocity * dt;
            t.position = math::clampToWorldBounds(t.position, world_width_, world_height_);
        } else if (e.type == types::EntityType::Bullet) {
            t.position += t.velocity * dt;
            if (std::abs(t.position.x) > half_w || std::abs(t.position.y) > half_h) {
                e.isDestroyed = true;
            }
        }
    }
}

void WorldInstance::resolveCollisions() {
    auto& storage = entities_.storage();
    const std::size_t n = storage.size();

    for (std::size_t i = 0; i < n; ++i) {
        auto& a_opt = storage[i];
        if (!a_opt || a_opt->isDestroyed || !a_opt->transform)
            continue;

        for (std::size_t j = i + 1; j < n; ++j) {
            if (a_opt->isDestroyed)
                break;
            auto& b_opt = storage[j];
            if (!b_opt || b_opt->isDestroyed || !b_opt->transform)
                continue;
            if (!mayCollide(a_opt->type, b_opt->type))
                continue;
            if (!math::collision::checkCircleCollision(a_opt->transform->position,
                                                       a_opt->radius,
                                                       b_opt->transform->position,
                                                       b_opt->radius)) {
                continue;
            }
            dispatchCollision(*a_opt, *b_opt);
        }
    }

    reapDestroyedShips();
}

bool WorldInstance::mayCollide(types::EntityType a, types::EntityType b) const noexcept {
    using T = types::EntityType;
    if (a == T::None || b == T::None)
        return false;
    if (a == T::Bullet && b == T::Bullet)
        return false;
    if (a == T::Asteroid && b == T::Asteroid)
        return false;
    return true;
}

void WorldInstance::dispatchCollision(domain::Entity& a, domain::Entity& b) {
    using T = types::EntityType;
    const T ta = a.type;
    const T tb = b.type;

    if (ta == T::Ship && tb == T::Ship)
        onCollisionShipShip(a, b);
    else if (ta == T::Ship && tb == T::Asteroid)
        onCollisionShipAsteroid(a, b);
    else if (ta == T::Asteroid && tb == T::Ship)
        onCollisionShipAsteroid(b, a);
    else if (ta == T::Bullet && tb == T::Ship)
        onCollisionBulletShip(a, b);
    else if (ta == T::Ship && tb == T::Bullet)
        onCollisionBulletShip(b, a);
    else if (ta == T::Bullet && tb == T::Asteroid)
        onCollisionBulletAsteroid(a, b);
    else if (ta == T::Asteroid && tb == T::Bullet)
        onCollisionBulletAsteroid(b, a);
}

void WorldInstance::onCollisionBulletShip(domain::Entity& proj, domain::Entity& ship) {
    if (!proj.combat || !ship.health || proj.combat->owner_id == ship.id) {
        return;
    }

    domain::collision::onCollisionBulletShip(proj, ship, [this](types::EntityId owner_id, bool ship_destroyed) {
        if (ship_destroyed) {
            if (auto* killer = entities_.findByEntityID(owner_id)) {
                if (killer->ship) {
                    killer->ship->frags += 1;
                    killer->ship->score += 100;
                    LOG_INFO("[WorldInstance] Killer {} scored a frag! Score: {}", owner_id.get(), killer->ship->score);
                }
            }
        }
    });
}

void WorldInstance::onCollisionBulletAsteroid(domain::Entity& proj, domain::Entity& asteroid) {
    domain::collision::onCollisionBulletAsteroid(proj, asteroid, [this, &asteroid](types::EntityId owner_id, bool asteroid_destroyed) {
        if (asteroid_destroyed) {
            LOG_INFO("[WorldInstance] Asteroid destroyed by bullet from player entity={}", owner_id.get());
            if (auto* killer = entities_.findByEntityID(owner_id)) {
                if (killer->ship && asteroid.asteroid) {
                    killer->ship->score += asteroid.asteroid->score_reward;
                }
            }
        }
    });
}

void WorldInstance::reapDestroyedShips() {
    auto& storage = entities_.storage();
    for (auto it = session_to_ship_.begin(); it != session_to_ship_.end();) {
        const types::EntityId eid = it->second;
        bool destroyed = false;
        for (auto& opt : storage) {
            if (opt && opt->id == eid) {
                destroyed = opt->isDestroyed || (opt->health && opt->health->current == 0);
                if (destroyed) {
                    opt->isDestroyed = true;
                }
                break;
            }
        }
        if (destroyed) {
            const SessionId sid = it->first;
            LOG_INFO("[WorldInstance] ship destroyed: session={}, entity={}", sid, eid.get());
            event_bus_->publish(ShipDestroyedEvent(sid));
            it = session_to_ship_.erase(it);
        } else {
            ++it;
        }
    }
}

void WorldInstance::onCollisionShipShip(domain::Entity& ship1, domain::Entity& ship2) {
    domain::collision::onCollisionShipShip(ship1, ship2);
}

void WorldInstance::onCollisionShipAsteroid(domain::Entity& ship, domain::Entity& asteroid) {
    domain::collision::onCollisionShipAsteroid(ship, asteroid);
}

void WorldInstance::maintainAsteroidPopulation() {
    std::size_t asteroid_count = 0;
    for (const auto& slot : entities_.storage()) {
        if (slot && !slot->isDestroyed && slot->type == types::EntityType::Asteroid) {
            ++asteroid_count;
        }
    }

    constexpr std::size_t kDesiredAsteroids = 20;
    while (asteroid_count < kDesiredAsteroids) {
        auto* asteroid = entities_.acquire();
        asteroid->type = types::EntityType::Asteroid;
        asteroid->mass = 15.0;
        asteroid->radius = 35.0;

        // Спавним астероиды случайным образом вдали от центра (0, 0)
        double angle = (static_cast<double>(asteroid_count) / kDesiredAsteroids) * 2.0 * std::numbers::pi;
        double dist = 400.0 + (asteroid_count * 45.0); // минимум 400px от центра

        math::Vector2D spawn_pos(std::cos(angle) * dist, std::sin(angle) * dist);
        // Медленный дрейф по касательной
        math::Vector2D drift_vel(-std::sin(angle) * 25.0, std::cos(angle) * 25.0);

        asteroid->transform = domain::TransformComponent{
            spawn_pos,
            drift_vel,
            angle
        };
        asteroid->health = domain::HealthComponent{100, 100};

        domain::AsteroidComponent comp;
        comp.score_reward = 50;
        asteroid->asteroid = comp;

        ++asteroid_count;
    }
}

void WorldInstance::spawnShips(const std::vector<PendingJoin>& new_players) {
    if (new_players.empty()) {
        return;
    }

    auto positions = freePositions(new_players.size(), world::kShipDefaultRadius * 2.0);
    if (positions.size() < new_players.size()) {
        LOG_WARN(
            "[WorldInstance] spawnShips: only {} free positions for {} players; "
            "falling back to largestEmptyCircle for the remaining",
            positions.size(),
            new_players.size());
    }

    for (std::size_t i = 0; i < new_players.size(); ++i) {
        const auto& join = new_players[i];
        if (session_to_ship_.contains(join.session_id)) {
            LOG_WARN("[WorldInstance] Duplicate spawn for session {}, ignoring", join.session_id);
            continue;
        }

        const math::Vector2D spawn_pos = (i < positions.size()) ? positions[i] : freePosition().center;

        auto* entity = entities_.acquire();
        entity->type = types::EntityType::Ship;
        entity->mass = world::kShipDefaultMass;
        entity->radius = world::kShipDefaultRadius;
        entity->transform = domain::TransformComponent{spawn_pos, math::Vector2D(0.0, 0.0), 0.0};

        domain::ShipComponent ship;
        ship.name.fill('\0');
        const std::size_t len = std::min(join.player_name.size(), ship.name.size() - 1);
        std::copy_n(join.player_name.data(), len, ship.name.begin());
        ship.thrust_power = world::kShipDefaultThrustPower;
        ship.drag = world::kShipDefaultDrag;
        entity->ship = ship;

        entity->health = domain::HealthComponent{world::kShipDefaultHealth, world::kShipDefaultHealth};
        entity->combat = domain::CombatComponent{world::kShipDefaultDamage, entity->id};

        session_to_ship_[join.session_id] = entity->id;

        const std::string accept_msg = std::format("ACCEPT {}\n", entity->id.get());
        std::vector<std::uint8_t> payload(accept_msg.begin(), accept_msg.end());
        event_bus_->publish(SendPacketEvent{join.session_id, std::move(payload)});

        LOG_INFO("[WorldInstance] Player '{}' joined/respawned: session={}, entity_id={}",
                 join.player_name, join.session_id, entity->id.get());
    }
}

std::vector<snapshot::LeaderboardEntry> WorldInstance::buildLeaderboard() const {
    std::vector<snapshot::LeaderboardEntry> top_players;
    top_players.reserve(entities_.storage().size());

    for (const auto& slot : entities_.storage()) {
        if (!slot.has_value())
            continue;
        const auto& entity = slot.value();

        if (entity.type == types::EntityType::Ship && entity.ship.has_value() && entity.health.has_value() &&
            entity.health->current > 0 && !entity.isDestroyed) {
            std::string_view view(entity.ship->name.data(), entity.ship->name.size());
            if (auto idx = view.find('\0'); idx != std::string_view::npos) {
                view = view.substr(0, idx);
            }
            top_players.emplace_back(std::string(view), entity.ship->score);
        }
    }

    const size_t top_count = std::min(world::kMaxLeaderboardSize, top_players.size());

    std::partial_sort(top_players.begin(),
                      top_players.begin() + top_count,
                      top_players.end(),
                      [](const auto& a, const auto& b) {
                          return a.score > b.score;
                      });

    top_players.resize(top_count);
    return top_players;
}

snapshot::GameSnapshot WorldInstance::buildGameSnapshot() {
    domain::buildEntitySnapshots(entities_, snapshot_buffer_);
    const auto tick_number = static_cast<std::uint32_t>(current_tick_.load());
    auto leaderboard = buildLeaderboard();
    // Порядок аргументов: snapshots, leaderboard, tick_number
    return snapshot::GameSnapshot{snapshot_buffer_, std::move(leaderboard), tick_number};
}

void WorldInstance::broadcastSnapshot(const snapshot::GameSnapshot& snapshot) {
    auto payload = snapshot.Serialize();
    event_bus_->publish(SendPacketEvent{NetworkResponseRouter::kBroadcastSessionId, std::move(payload)});
}

void WorldInstance::setupSubscriptions() {
    auto weak_self = weak_from_this();

    sub_disconnect_ = event_bus_->subscribe<ClientDisconnectedEvent>([weak_self](const ClientDisconnectedEvent& event) {
        if (auto world = weak_self.lock()) {
            world->onClientDisconnected(event);
        }
    });

    sub_player_join_ = event_bus_->subscribe<PlayerJoinEvent>([weak_self](const PlayerJoinEvent& event) {
        if (auto world = weak_self.lock()) {
            world->onPlayerJoin(event);
        }
    });

    sub_player_move_ = event_bus_->subscribe<PlayerMoveEvent>([weak_self](const PlayerMoveEvent& event) {
        if (auto world = weak_self.lock()) {
            world->onPlayerMove(event);
        }
    });

    sub_player_shoot_ = event_bus_->subscribe<PlayerShootEvent>([weak_self](const PlayerShootEvent& event) {
        if (auto world = weak_self.lock()) {
            world->onPlayerShoot(event);
        }
    });

    sub_player_respawn_ = event_bus_->subscribe<PlayerRespawnEvent>([weak_self](const PlayerRespawnEvent& event) {
        if (auto world = weak_self.lock()) {
            world->onPlayerRespawn(event);
        }
    });
}

void WorldInstance::onClientDisconnected(const ClientDisconnectedEvent& event) {
    ActionEvent action;
    action.type = ActionType::PlayerQuit;
    action.session_id = event.session_id;
    enqueueAction(std::move(action));
    LOG_INFO("[WorldInstance] PlayerQuit for session {}", event.session_id);
}

void WorldInstance::onPlayerJoin(const PlayerJoinEvent& event) {
    ActionEvent action;
    action.type = ActionType::PlayerJoin;
    action.session_id = event.session_id;
    action.player_name = event.login;
    enqueueAction(std::move(action));
    LOG_INFO("[WorldInstance] PlayerJoin: {} join to game from session {}", event.login, event.session_id);
}

void WorldInstance::onPlayerMove(const PlayerMoveEvent& event) {
    ActionEvent action;
    action.type = ActionType::PlayerMove;
    action.session_id = event.session_id;
    action.angle = event.angle;
    enqueueAction(std::move(action));
}

void WorldInstance::onPlayerShoot(const PlayerShootEvent& event) {
    ActionEvent action;
    action.type = ActionType::PlayerShoot;
    action.session_id = event.session_id;
    enqueueAction(std::move(action));
}

void WorldInstance::onPlayerRespawn(const PlayerRespawnEvent& event) {
    ActionEvent action;
    action.type = ActionType::PlayerRespawn;
    action.session_id = event.session_id;
    enqueueAction(std::move(action));
}

}  // namespace domain

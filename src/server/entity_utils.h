#pragma once

#include <cmath>
#include <optional>
#include <string>
#include <type_traits>

#include "entity.h"
#include "entity_snapshot.h"

namespace domain {

/**
 * @brief Преобразует доменную сущность в снимок для рассылки клиентам.
 * @details Переносит все компоненты 1:1: transform, ship, health, combat, asteroid.
 *          Имя корабля копируется из NUL-terminated массива в std::string.
 * @param entity Входные данные: Доменная сущность доменного мира.
 * @return snapshot::EntitySnapshot Снимок сущности.
 */
[[nodiscard]] inline snapshot::EntitySnapshot toEntitySnapshot(const Entity& entity) {
    snapshot::EntitySnapshot result;
    result.id = entity.id;
    result.type = entity.type;
    result.mass = entity.mass;
    result.radius = entity.radius;
    result.is_destroyed = entity.isDestroyed;

    if (entity.transform) {
        result.transform = snapshot::TransformSnapshot{entity.transform->position,
                                                       entity.transform->velocity,
                                                       entity.transform->angle};
    }
    if (entity.ship) {
        result.ship = snapshot::ShipSnapshot{std::string{entity.ship->name.data()},
                                             entity.ship->score,
                                             entity.ship->thrust_power,
                                             entity.ship->drag,
                                             entity.ship->is_thrusting,
                                             entity.ship->frags};
    }
    if (entity.health) {
        result.health = snapshot::HealthSnapshot{entity.health->current, entity.health->max};
    }
    if (entity.combat) {
        result.combat = snapshot::CombatSnapshot{entity.combat->damage, entity.combat->owner_id};
    }
    if (entity.asteroid) {
        result.asteroid = snapshot::AsteroidSnapshot{entity.asteroid->score_reward};
    }
    return result;
}

/**
 * @brief Строит снимки всех живых сущностей из хранилища пула.
 * @details Пустые слоты и сущности с флагом isDestroyed пропускаются.
 * @tparam EntityContainer Контейнер Entity или std::optional<Entity>
 *         (работает с EntityPool::storage() напрямую).
 * @param storage Входные данные: Хранилище сущностей.
 * @return std::vector<snapshot::EntitySnapshot> Снимки сущностей.
 */
template <typename EntityContainer>
[[nodiscard]] inline std::vector<snapshot::EntitySnapshot> buildEntitySnapshots(const EntityContainer& storage) {
    std::vector<snapshot::EntitySnapshot> snapshots;
    snapshots.reserve(storage.size());

    for (const auto& slot : storage) {
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(slot)>, std::optional<Entity>>) {
            if (!slot || slot->isDestroyed) {
                continue;
            }
            snapshots.push_back(toEntitySnapshot(*slot));
        } else {
            if (slot.isDestroyed) {
                continue;
            }
            snapshots.push_back(toEntitySnapshot(slot));
        }
    }

    return snapshots;
}

/**
 * @brief Integrate motion for all live entities and resolve world bounds.
 *
 * @tparam EntityContainer Range of Entity or std::optional<Entity>.
 *         Works with EntityPool::storage() directly.
 *
 * @param entities     Container of entities. Empty optionals, destroyed
 *                     entities, and entities without a transform are skipped.
 * @param dt           Frame duration.
 *
 * @pre dt >= 0, world_width > 0, world_height > 0.
 *
 * @post Bullets that leave the world are marked isDestroyed = true.
 * @post Ships are clamped to the world rectangle; wall hits zero only the
 *       velocity component normal to the wall (sliding).
 * @post Asteroids wrap around the world edges once their full radius has
 *       left the world rectangle.
 *
 * @note Uses discrete integration; fast bullets can tunnel through small
 *       targets.
 * @note Mutates entity.isDestroyed but does not reclaim storage; call
 *       EntityPool::recycleDestroyed() once per frame afterwards.
 */
template <typename EntityContainer>
inline void updateMovementAndBounds(EntityContainer& entities,
                                    math::Scalar dt,
                                    math::Scalar world_width,
                                    math::Scalar world_height) {
    using ValueType = typename EntityContainer::value_type;

    for (auto& slot : entities) {
        // Unwrap std::optional<Entity> slots (EntityPool::storage()).
        Entity* entity_ptr = nullptr;
        if constexpr (std::is_same_v<ValueType, std::optional<Entity>>) {
            if (!slot.has_value()) {
                continue;
            }
            entity_ptr = &*slot;
        } else {
            entity_ptr = &slot;
        }

        Entity& entity = *entity_ptr;

        if (entity.type == types::EntityType::None || entity.isDestroyed || !entity.transform.has_value()) {
            continue;
        }

        auto& t = *entity.transform;

        // Integrate.
        if (entity.type == types::EntityType::Ship && entity.ship.has_value()) {
            auto& s = *entity.ship;
            math::updateShipPhysics(t.position, t.velocity, t.angle, s.is_thrusting, s.thrust_power, s.drag, dt);
        } else {
            t.position += t.velocity * dt;
        }

        // Asteroids wrap once fully outside the world.
        if (entity.type == types::EntityType::Asteroid) {
            const math::Scalar r = entity.radius;

            if (t.position.x + r < 0.0) {
                t.position.x = world_width + r;
            } else if (t.position.x - r > world_width) {
                t.position.x = -r;
            }

            if (t.position.y + r < 0.0) {
                t.position.y = world_height + r;
            } else if (t.position.y - r > world_height) {
                t.position.y = -r;
            }

            continue;
        }

        // Clamp for everything else.
        const math::Vector2D clamped = math::clampToWorldBounds(t.position, world_width, world_height);

        // Epsilon-safe comparison: did clamping actually move us on each axis?
        const bool clamped_x = std::abs(clamped.x - t.position.x) > math::kEps;
        const bool clamped_y = std::abs(clamped.y - t.position.y) > math::kEps;

        if (!clamped_x && !clamped_y) {
            continue;
        }

        switch (entity.type) {
            case types::EntityType::None:
                // Safety net; unreachable.
                break;

            case types::EntityType::Bullet:
                entity.isDestroyed = true;
                break;

            case types::EntityType::Ship:
                if (clamped_x) {
                    t.velocity.x = 0.0;
                }
                if (clamped_y) {
                    t.velocity.y = 0.0;
                }
                t.position = clamped;
                break;

            case types::EntityType::Asteroid:
                // Unreachable: asteroids are handled (wrapped) above.
                break;
        }
    }
}

}  // namespace domain

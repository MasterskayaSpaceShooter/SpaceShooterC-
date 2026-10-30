#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <type_traits>

#include "entity.h"
#include "entity_pool.h"
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
        const auto* name_begin = entity.ship->name.data();
        const auto* name_end = entity.ship->name.data() + entity.ship->name.size();
        const auto* nul = std::find(name_begin, name_end, '\0');
        result.ship = snapshot::ShipSnapshot{std::string{name_begin, nul},
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
 * @brief Заполняет буфер снимками всех живых сущностей из пула.
 * @details Пустые слоты, сущности с флагом isDestroyed и типом EntityType::None
 *          пропускаются (см. EntityPool::isActive). Ёмкость буфера переиспользуется
 *          между вызовами (например, между тиками), чтобы не аллоцировать вектор
 *          на каждый кадр. Верхняя граница числа снимков берётся из
 *          EntityPool::activeCount() (O(1), без прохода по хранилищу).
 * @param pool Входные данные: Пул сущностей.
 * @param out_snapshots Входные данные: Буфер для снимков, переиспользуется между вызовами.
 * @return std::size_t Число записанных снимков (размер буфера после вызова).
 * @pre Не потокобезопасна. Вызов не содержит внутренней синхронизации: читает
 *      pool и мутирует out_snapshots напрямую. Доступ к обоим аргументам должен
 *      быть сериализован между потоками вызывающим кодом (в проекте это
 *      обеспечивает tick_strand_ WorldInstance). Чтение пула небезопасно при
 *      конкурентном EntityPool::acquire(), EntityPool::recycleDestroyed() или
 *      прямой записи в EntityPool::storage().
 */
inline std::size_t buildEntitySnapshots(const EntityPool& pool, std::vector<snapshot::EntitySnapshot>& out_snapshots) {
    out_snapshots.clear();
    out_snapshots.reserve(pool.activeCount());

    const auto& storage = pool.storage();
    for (const auto& slot : storage) {
        if (pool.isActive(slot)) {
            out_snapshots.push_back(toEntitySnapshot(*slot));
        }
    }

    return out_snapshots.size();
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

        // Asteroids wrap through the world edge. After the wrap, the center sits
        // just outside the opposite edge (center at ±(w/2 + r)), so the body
        // re-enters over the following ticks.
        if (entity.type == types::EntityType::Asteroid) {
            t.position = math::wrapCircleThroughWorldBounds(t.position, entity.radius, world_width, world_height);
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

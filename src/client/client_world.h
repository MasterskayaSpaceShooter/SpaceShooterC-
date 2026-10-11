#pragma once

#include <cstdint>
#include <unordered_map>

#include "game_snapshot.h"

namespace client {

/// Уникальный идентификатор игровой сущности (корабля, снаряда и т.п.).
using EntityId = std::uint32_t;

/**
 * @brief Снимок состояния одной сущности на клиенте.
 * @details Заглушка — поля будут уточнены по мере развития игрового протокола.
 */
struct EntitySnapshot {
    EntityId id{0};
};

/**
 * @brief Клиентское представление игрового мира.
 * @details Зона ответственности:
 *          - Хранит локальную копию сущностей, полученных от сервера.
 *          - Знает, какой EntityId принадлежит локальному игроку.
 *          - Предоставляет доступ к сущностям для рендера.
 *
 *          Реализация — заглушка: данные от сервера ещё не приходят,
 *          логика обновления будет добавлена позже.
 */
class ClientWorld {
public:
    ClientWorld() = default;

    /**
     * @brief Обновляет локальный мир из снимка состояния сервера.
     * @param snapshot Входные данные: Снимок состояния мира от сервера.
     * @param my_player_id Входные данные: EntityId локального игрока.
     * @note Заглушка — пока ничего не делает.
     */
    void updateSnapshot(const GameSnapshot& snapshot, EntityId my_player_id);

    /**
     * @brief Жив ли корабль локального игрока.
     * @return true если сущность игрока присутствует в entities_.
     * @note Заглушка — пока всегда false.
     */
    [[nodiscard]] bool isMyPlayerAlive() const;

    /**
     * @brief Возвращает карту всех известных сущностей мира.
     */
    [[nodiscard]] const std::unordered_map<EntityId, EntitySnapshot>& getEntities() const noexcept {
        return entities_;
    }

    /**
     * @brief Возвращает EntityId локального игрока.
     */
    [[nodiscard]] EntityId myPlayerId() const noexcept {
        return my_player_id_;
    }

private:
    EntityId my_player_id_{0};
    std::unordered_map<EntityId, EntitySnapshot> entities_;
};
}  // namespace client

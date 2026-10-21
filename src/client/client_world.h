#pragma once

#include <unordered_map>

#include "entity_snapshot.h"
#include "game_snapshot.h"
#include "types.h"

namespace client {
class ClientWorld {
public:
    ClientWorld() = default;

    /**
     * @brief Обновляет локальный мир из снимка состояния сервера.
     * @param snapshot Входные данные: Снимок состояния мира от сервера.
     * @param my_player_id Входные данные: EntityId локального игрока.
     * @note Заглушка — пока ничего не делает.
     */
    void updateSnapshot(const snapshot::GameSnapshot& snapshot, types::EntityId my_player_id);

    /**
     * @brief Жив ли корабль локального игрока.
     * @return true если сущность игрока присутствует в entities_.
     * @note Заглушка — пока всегда false.
     */
    [[nodiscard]] bool isMyPlayerAlive() const;

    /**
     * @brief Возвращает карту всех известных сущностей мира.
     */
    [[nodiscard]] const std::unordered_map<types::EntityId, snapshot::EntitySnapshot>& getEntities() const noexcept {
        return entities_;
    }

    /**
     * @brief Возвращает EntityId локального игрока.
     */
    [[nodiscard]] types::EntityId myPlayerId() const noexcept {
        return my_player_id_;
    }

private:
    types::EntityId my_player_id_{0};
    std::unordered_map<types::EntityId, snapshot::EntitySnapshot> entities_;
};
}  // namespace client

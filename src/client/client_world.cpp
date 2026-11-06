#include "client_world.h"

#include "death_screen.h"

namespace client {

void ClientWorld::updateSnapshot(const snapshot::GameSnapshot& game_snapshot, types::EntityId my_player_id) {
    my_player_id_ = my_player_id;
    leaderboard_ = game_snapshot.getLeaderboard();

    // Обновляем карту сущностей из снимка
    entities_.clear();
    for (const auto& entity : game_snapshot.getSnapshots()) {
        entities_.emplace(entity.id, entity);
    }
}

void ClientWorld::updateDeathScreen(DeathScreen& death_screen) const {
    // Активируем экран, если игрок мертв, и деактивируем, если жив (respawn)
    death_screen.setActive(!isMyPlayerAlive());
}

bool ClientWorld::isMyPlayerAlive() const {
    const auto it = entities_.find(my_player_id_);
    if (it == entities_.end()) {
        return false;
    }
    // Проверка на уничтожение
    return !it->second.is_destroyed;
}

}  // namespace client

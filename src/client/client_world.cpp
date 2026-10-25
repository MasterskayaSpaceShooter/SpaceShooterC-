#include "client_world.h"

namespace client {

void ClientWorld::updateSnapshot(const snapshot::GameSnapshot& game_snapshot, types::EntityId my_player_id) {
    my_player_id_ = my_player_id;
    leaderboard_ = game_snapshot.getLeaderboard();
}

bool ClientWorld::isMyPlayerAlive() const {
    return false;
}

}  // namespace client

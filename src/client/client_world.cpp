#include "client_world.h"

namespace client {

void ClientWorld::updateSnapshot(const GameSnapshot& /*snapshot*/, EntityId my_player_id) {
    // Разобрать снимок и обновить entities_.
    my_player_id_ = my_player_id;
}

bool ClientWorld::isMyPlayerAlive() const {
    // Вернуть true, если my_player_id_ есть в entities_.
    return false;
}
}  // namespace client

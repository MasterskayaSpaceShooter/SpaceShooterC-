// common_lib.cpp — исходный файл статической библиотеки common.
//
// Здесь будут реализованы общие для сервера и клиента компоненты:
// - класс Asteroid
// - класс Spaceship
// - класс GameSnapshot

#include "common_lib.h"

#include "Asteroid.h"
#include "GameSnapshot.h"
#include "Spaceship.h"

namespace common_lib {
std::string getCommandName(GameCommand cmd) {
    switch (cmd) {
        case GameCommand::JoinGame:
            return "JOIN_GAME";
        case GameCommand::Move:
            return "MOVE";
        case GameCommand::Shoot:
            return "SHOOT";
        case GameCommand::LeaveGame:
            return "LEAVE_GAME";
        default:
            return "UNKNOWN";
    }
}

// TODO: реализация общих функций и классов игры

}  // namespace common_lib
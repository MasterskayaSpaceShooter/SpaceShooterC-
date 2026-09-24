#pragma once
#include <cstdint>
#include <string>

namespace common_lib {
// Типы игровых пакетов
enum class GameCommand : uint8_t { JoinGame = 1, Move, Shoot, LeaveGame };

// Базовая структура сетевого сообщения
struct NetworkPacket {
    GameCommand command;
    uint32_t playerId;
    std::string payload;
};

// Функция-заглушка для проверки работы библиотеки
std::string getCommandName(GameCommand cmd);
}  // namespace common_lib

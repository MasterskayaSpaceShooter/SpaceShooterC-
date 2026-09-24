#include <asio.hpp>
#include <common/protocol.hpp>
#include <iostream>

int main() {
    std::cout << "[Client] SpaceShooterC- Client Connected." << std::endl;
    std::cout << "[Client] Protocol Check: " << common::getCommandName(common::GameCommand::Shoot) << std::endl;
    return 0;
}

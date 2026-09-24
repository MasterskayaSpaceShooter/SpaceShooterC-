#include <boost/asio.hpp>
#include <iostream>

int main() {
    try {
        boost::asio::io_context io_context;

        std::cout << "[Server] SpaceShooterC- Server Started." << std::endl;

        // В следующих шагах здесь будет асинхронный прием подключений
        io_context.run();
    } catch (const std::exception& e) {
        std::cerr << "[Server] Exception: " << e.what() << std::endl;
    }
    return 0;
}

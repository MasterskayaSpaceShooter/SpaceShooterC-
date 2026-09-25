#pragma once
#include <boost/asio.hpp>
#include <thread>
#include <vector>

/**
 * @brief Менеджер потокового контекста ввода-вывода (Boost.Asio Execution Context).
 * @details Зона ответственности:
 *          - Владение единым `boost::asio::io_context`.
 *          - Управление пулом рабочих потоков `std::jthread` (C++20).
 *          - Гарантия корректной остановки цикла событий (Graceful Shutdown).
 */
class NetworkContext {
public:
    /**
     * @brief Конструктор сетевого контекста.
     * @details Взаимодействует с членами класса: инициализирует io_context_, thread_count_ и work_guard_.
     * @param thread_count Входные данные: Количество рабочих потоков в пуле (по умолчанию равно числу ядер CPU).
     */
    explicit NetworkContext(size_t thread_count = std::thread::hardware_concurrency());

    /**
     * @brief Деструктор сетевого контекста.
     * @details Взаимодействует с членами класса: автоматически вызывает метод stop() для корректного завершения
     * потоков.
     */
    ~NetworkContext();

    NetworkContext(const NetworkContext&) = delete;
    NetworkContext& operator=(const NetworkContext&) = delete;

    /**
     * @brief Запускает рабочий пул потоков std::jthread и выполняет io_context_.run() в каждом из них.
     * @details Взаимодействует с полем: worker_threads_, io_context_, thread_count_.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void start();

    /**
     * @brief Останавливает io_context и дожидается корректного завершения всех рабочих потоков.
     * @details Взаимодействует с полем: work_guard_, io_context_, worker_threads_.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void stop();

    /**
     * @brief Возвращает ссылку на внутренний контекст ввода-вывода Asio.
     * @details Взаимодействует с полем: io_context_.
     * @inputs Входных параметров нет.
     * @return boost::asio::io_context& Ссылка на используемый контекст событий.
     */
    [[nodiscard]] boost::asio::io_context& getContext() noexcept {
        return io_context_;
    }

private:
    boost::asio::io_context io_context_;  ///< Главный контекст событий Asio
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        work_guard_;                            ///< Защитник от пустой остановки run()
    std::vector<std::jthread> worker_threads_;  ///< Пул рабочих потоков
    size_t thread_count_;                       ///< Целевое количество потоков
};
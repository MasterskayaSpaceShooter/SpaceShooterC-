#pragma once
#include <algorithm>
#include <atomic>
#include <boost/asio.hpp>
#include <cassert>
#include <iostream>
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
    explicit NetworkContext(size_t thread_count = std::thread::hardware_concurrency()) :
        work_guard_(boost::asio::make_work_guard(io_context_)), thread_count_(std::max<std::size_t>(1, thread_count)) {}

    /**
     * @brief Деструктор сетевого контекста.
     * @details Взаимодействует с членами класса: автоматически вызывает метод request_stop() и wait() для корректного
     * завершения потоков.
     */
    ~NetworkContext() {
        request_stop();
        wait();
    }

    NetworkContext(const NetworkContext&) = delete;
    NetworkContext& operator=(const NetworkContext&) = delete;

    /**
     * @brief Запускает рабочий пул потоков std::jthread и выполняет io_context_.run() в каждом из них.
     * @details Взаимодействует с полем: worker_threads_, io_context_, thread_count_.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void start() {
        if (started_.exchange(true)) {
            return;
        }
        worker_threads_.reserve(thread_count_);
        for (size_t i = 0; i < thread_count_; ++i) {
            worker_threads_.emplace_back([this](std::stop_token st) {
                while (!st.stop_requested()) {
                    try {
                        io_context_.run();
                    } catch (const std::exception& e) {
                        std::cerr << "[NetworkContext] worker exception: " << e.what() << '\n';
                    } catch (...) {
                        std::cerr << "[NetworkContext] worker unknown exception\n";
                    }
                    if (io_context_.stopped()) {
                        break;
                    }
                }
            });
        }
    }

    /**
     * @brief Сигнализирует воркерам о необходимости остановки. Не блокирует.
     * @details Снимает work_guard, выставляет stop_token каждому jthread, останавливает io_context. Безопасно вызывать
     * из любого потока, включая воркеры.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void request_stop() noexcept {
        if (!started_.exchange(false)) {
            return;
        }
        work_guard_.reset();
        for (auto& t : worker_threads_) {
            t.request_stop();
        }
        io_context_.stop();
    }

    /**
     * @brief Блокирующе дожидается завершения всех воркеров.
     * @details Взаимодействует с полем: worker_threads_.
     * @pre started_ == false — перед вызовом должен быть вызван request_stop().
     * @pre Вызов из потока, не являющегося воркером (иначе self-join → terminate).
     * @note Нарушение предусловий — assert в debug, неопределённое поведение в release.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void wait() {
        assert(!started_.load() && "NetworkContext::wait() called before request_stop()");
        assert(!is_worker_thread() && "NetworkContext::wait() must not be called from a worker thread");

        for (auto& t : worker_threads_) {
            if (t.joinable())
                t.join();
        }
        worker_threads_.clear();
    }

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
    bool is_worker_thread() const noexcept {
        const auto id = std::this_thread::get_id();
        return std::any_of(worker_threads_.begin(), worker_threads_.end(), [id](const std::jthread& t) {
            return t.get_id() == id;
        });
    }

    boost::asio::io_context io_context_;  ///< Главный контекст событий Asio
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        work_guard_;                            ///< Защитник от пустой остановки run()
    std::vector<std::jthread> worker_threads_;  ///< Пул рабочих потоков
    size_t thread_count_;                       ///< Целевое количество потоков
    std::atomic<bool> started_{false};          ///< Флаг состояния пула
};

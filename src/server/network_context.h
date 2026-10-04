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
 * @note Одноразовый жизненный цикл: start() можно вызвать только один раз.
 *       После stop()/force_stop()/деструктора повторный start() не
 *       восстановит работу io_context. Для перезапуска создаётся новый
 *       NetworkContext.
 * @note Два режима остановки:
 *        stop()       - graceful shutdown: снимает keep-alive, даёт io_context
 *                        доработать очередь и активные операции.
 *                        Может зависнуть на wait(), если операции не завершаются.
 *        force_stop() - аварийный: обрывает очередь и активные операции.
 *                        Текущий handler доработает, остальные  теряются.
 * @note Потокобезопасность:
 *        start(), stop(), force_stop() потокобезопасны относительно
 *        самих себя. wait() не потокобезопасен. Должен вызываться только из
 *        потока-владельца, после завершения всех start()/stop()/force_stop() из других потоков.
 *        Одновременный вызов stop() и force_stop() из разных потоков - data race на work_guard_. Не допускается.
 *        Одновременный вызов start() и wait() - data race на worker_threads_. Не допускается.
 *        is_worker_thread() читает worker_threads_ без блокировки.
 *        Корректен только когда вектор не модифицируется параллельно.
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
     * @details Взаимодействует с членами класса: автоматически вызывает метод force_stop() и wait() для корректного
     * завершения потоков.
     */
    ~NetworkContext() {
        force_stop();
        wait();
    }

    NetworkContext(const NetworkContext&) = delete;
    NetworkContext& operator=(const NetworkContext&) = delete;

    /**
     * @brief Запускает пул воркеров. Идемпотентно в пределах жизненного цикла.
     * @details Создаёт thread_count_ потоков std::jthread, каждый из которых
     *          выполняет io_context_.run() в цикле до остановки.
     *          При исключении в процессе создания потоков выполняется откат:
     *          started_ сбрасывается, созданные потоки останавливаются и джойнятся.
     * @note Одноразовый: после stop() или force_stop() повторный start() не восстановит io_context.
     * Для перезапуска создаётся новый NetworkContext.
     * @throws std::system_error при невозможности создать поток.
     *         Состояние объекта откатывается к «не запущен».
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void start() {
        if (started_.exchange(true)) {
            return;
        }

        try {
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
        } catch (...) {
            started_.store(false);

            for (auto& t : worker_threads_) {
                t.request_stop();
            }
            io_context_.stop();

            for (auto& t : worker_threads_) {
                if (t.joinable())
                    t.join();
            }
            worker_threads_.clear();

            throw;
        }
    }

    /**
     * @brief Мягкая остановка: даёт io_context доработать очередь.
     * @details Снимает work_guard. run() вернётся, когда все асинхронные операции и таймеры завершатся сами.
     *          Безопасно вызывать из любого потока, включая воркеры.
     * @warning Может зависнуть на wait(), если активные async-операции не завершаются.
     *          Для аварийного завершения использовать force_stop().
     */
    void stop() noexcept {
        started_.store(false);
        work_guard_.reset();
    }

    /**
     * @brief Аварийная остановка: обрывает очередь и активные операции.
     * @details Снимает work_guard, выставляет stop_token каждому jthread, вызывает io_context_.stop().
     *          Текущий handler доработает, остальные — не выполнятся. Активные async-операции не завершатся.
     *          Безопасно вызывать из любого потока, включая воркеры.
     * @warning Pending handler'ы теряются.
     */
    void force_stop() noexcept {
        started_.store(false);
        work_guard_.reset();
        for (auto& t : worker_threads_) {
            t.request_stop();
        }
        io_context_.stop();
    }

    /**
     * @brief Блокирующе дожидается завершения всех воркеров.
     * @details Взаимодействует с полем: worker_threads_.
     * @pre started_ == false — перед вызовом должен быть вызван stop() или force_stop().
     * @pre Вызов из потока, не являющегося воркером (иначе self-join → terminate).
     * @note Нарушение предусловий — assert в debug, неопределённое поведение в release.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void wait() {
        assert(!started_.load() && "NetworkContext::wait() called before stop() / force_stop()");
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
     * @warning Ссылка действительна только пока живёт NetworkContext.
     *          Не сохранять её в объектах, переживающих NetworkContext, это приведёт к use-after-free.
     * @warning не вызывать через эту ссылку:
     *          run()     - жизненным циклом воркеров управляет только NetworkContext;
     *          stop()    - использовать force_stop();
     *          restart() - жизненный цикл одноразовый.
     *          Это нарушит инварианты NetworkContext и приведёт к
     *          неопределённому поведению при stop()/force_stop()/wait().
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
    std::size_t thread_count_;                  ///< Целевое количество потоков
    std::atomic<bool> started_{false};          ///< Флаг состояния пула
};

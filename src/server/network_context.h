#pragma once
#include <algorithm>
#include <boost/asio.hpp>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

/**
 * @brief Менеджер потокового контекста ввода-вывода (Boost.Asio Execution Context).
 * @details Зона ответственности:
 *          - Владение единым `boost::asio::io_context`.
 *          - Управление пулом рабочих потоков `std::jthread` (C++20).
 * @note Одноразовый жизненный цикл. Допустимые переходы состояний:
 *          NotStarted --start()--> Started
 *          Started    --stop()/force_stop()--> Stopped
 *          Stopped    --wait()--> Joined
 *          NotStarted --wait()--> abort (нарушение предусловия)
 *          NotStarted --~NetworkContext()--> безопасно (no-op + join пустого вектора)
 *          Joined     --wait()--> no-op
 *       Повторный start() после Stopped/Joined бросает std::logic_error. Для перезапуска создаётся новый
 *       NetworkContext.
 * @note Два режима остановки:
 *        stop()       - graceful shutdown: снимает keep-alive и выставляет
 *                       stop_token каждому jthread, даёт io_context
 *                        доработать очередь и активные операции.
 *                        Может зависнуть на wait(), если операции не завершаются.
 *        force_stop() - аварийный: обрывает очередь и активные операции.
 *                        Текущий handler доработает, остальные  теряются.
 * @note Потокобезопасность:
 *        start(), stop(), force_stop(), wait() синхронизированы общим
 *        std::mutex и безопасны при вызове из разных потоков.
 *        Единственное исключение: wait() и деструктор запрещено вызывать
 *        из воркер-потока этого же контекста (self-join) - детектируется
 *        через thread_local метку и приводит к std::abort.
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
     * @details Взаимодействует с членами класса: автоматически вызывает метод  и приватный join_workers()
     *          (эквивалент wait() без проверки предусловий), чтобы уничтожение
     *          никогда не стартовавшего объекта не приводило к abort().
     * @warning Не вызывать из воркер-потока - будет std::abort.
     */
    ~NetworkContext() {
        if (is_worker_thread()) {
            fatal("~NetworkContext() called from a worker thread");
        }
        force_stop();
        join_workers();
    }

    NetworkContext(const NetworkContext&) = delete;
    NetworkContext& operator=(const NetworkContext&) = delete;

    /**
     * @brief Запускает пул воркеров. Идемпотентен, только если уже вызван и
     *        пул работает (state_ == Started); в остальных состояниях см. @throws.
     * @details Создаёт thread_count_ потоков std::jthread, каждый из которых
     *          выполняет io_context_.run() в цикле до получения stop_token
     *          или остановки io_context.
     *          При исключении в процессе создания потоков выполняется откат:
     *          состояние переводится в Stopped, созданные потоки получают
     *          request_stop() и джойнятся вне блокировки мьютекса.
     * @throws std::logic_error - если start() вызывается после stop()/force_stop()/wait().
     * @throws std::system_error - при невозможности создать поток (после отката).
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void start() {
        std::unique_lock lock(mutex_);

        switch (state_) {
            case State::Started: {
                return;
            }
            case State::Stopped:
            case State::Joined: {
                throw std::logic_error("NetworkContext: start() after stop() — create a new instance");
            }
            case State::NotStarted: {
                break;
            }
        }

        state_ = State::Started;

        try {
            worker_threads_.reserve(thread_count_);
            for (size_t i = 0; i < thread_count_; ++i) {
                worker_threads_.emplace_back([this](std::stop_token st) {
                    worker_loop(st);
                });
            }
        } catch (...) {
            // откат
            for (auto& t : worker_threads_) {
                t.request_stop();
            }
            io_context_.stop();
            auto to_join = std::move(worker_threads_);
            worker_threads_.clear();
            state_ = State::Stopped;

            lock.unlock();
            throw;
        }
    }

    /**
     * @brief Мягкая остановка: даёт io_context доработать очередь.
     * @details Снимает work_guard и выставляет stop_token каждому воркеру.
     *          run() вернётся, когда очередь опустеет; воркеры выйдут из цикла
     *          по stop_requested(). Повторный вызов - no-op.
     *          Безопасно вызывать из любого потока, включая воркеры.
     * @warning Может зависнуть на wait(), если активные async-операции не завершаются.
     *          Для аварийного завершения использовать force_stop().
     */
    void stop() noexcept {
        std::lock_guard lock(mutex_);
        if (state_ != State::Started) {
            return;  // no-op до start() или после stop()
        }
        state_ = State::Stopped;
        work_guard_.reset();  // run() вернётся, когда очередь опустеет
        for (auto& t : worker_threads_) {
            t.request_stop();  // ← ключевое: цикл воркера увидит и выйдет
        }
    }

    /**
     * @brief Аварийная остановка: обрывает очередь и активные операции.
     * @details Снимает work_guard, выставляет stop_token каждому jthread, вызывает io_context_.stop().
     *          Текущий handler доработает, остальные — не выполнятся. Активные async-операции не завершатся.
     *          Повторный вызов - no-op.
     *          Безопасно вызывать из любого потока, включая воркеры.
     * @warning Pending handler'ы теряются.
     */
    void force_stop() noexcept {
        std::lock_guard lock(mutex_);
        if (state_ != State::Started) {
            return;
        }
        state_ = State::Stopped;
        io_context_.stop();
        work_guard_.reset();
        for (auto& t : worker_threads_)
            t.request_stop();
    }

    /**
     * @brief Блокирующе дожидается завершения всех воркеров.
     * @details Переносит worker_threads_ под мьютексом, затем join вне
     *          блокировки, чтобы не задедлочить воркер, вызывающий stop().
     * @pre Перед вызовом должен быть вызван stop() или force_stop()
     *      (state_ != Started и != NotStarted).
     * @pre Вызов из потока, не являющегося воркером (иначе self-join).
     * @note Нарушение предусловий - std::abort с диагностикой в stderr.
     * @note Повторный вызов после Joined - no-op.
     * @note Не вызывать из деструктора: деструктор использует приватный
     *       join_workers() без проверки предусловий, чтобы уничтожение
     *       никогда не стартовавшего объекта не приводило к abort().
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void wait() {
        if (is_worker_thread()) {
            fatal("NetworkContext::wait() called from a worker thread");
        }
        {
            std::lock_guard lock(mutex_);
            if (state_ == State::Started || state_ == State::NotStarted) {
                fatal("NetworkContext::wait() called before stop() / force_stop()");
            }
        }
        join_workers();
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
    enum class State : std::uint8_t { NotStarted, Started, Stopped, Joined };

    inline static thread_local NetworkContext* tls_worker_ctx_ = nullptr;

    [[nodiscard]] bool is_worker_thread() const noexcept {
        return tls_worker_ctx_ == this;
    }

    void join_workers() {
        std::vector<std::jthread> to_join;
        {
            std::lock_guard lock(mutex_);
            if (state_ == State::Joined)
                return;
            to_join = std::move(worker_threads_);
            worker_threads_.clear();
            state_ = State::Joined;
        }
    }

    [[noreturn]] static void fatal(const char* msg) {
        std::cerr << "[NetworkContext] FATAL: " << msg << '\n';
        std::abort();
    }

    void worker_loop(std::stop_token st) noexcept {
        struct Cleanup {
            ~Cleanup() {
                tls_worker_ctx_ = nullptr;
            }
        } cleanup;
        tls_worker_ctx_ = this;

        while (!st.stop_requested()) {
            try {
                io_context_.run();
            } catch (const std::exception& e) {
                std::cerr << "[NetworkContext] worker exception: " << e.what() << '\n';
            } catch (...) {
                std::cerr << "[NetworkContext] worker unknown exception\n";
            }
            if (io_context_.stopped() || st.stop_requested())
                break;
        }
    }

    boost::asio::io_context io_context_;  ///< Главный контекст событий Asio
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        work_guard_;                            ///< Защитник от пустой остановки run()
    std::vector<std::jthread> worker_threads_;  ///< Пул рабочих потоков
    const std::size_t thread_count_;            ///< Целевое количество потоков
    mutable std::mutex mutex_;
    State state_{State::NotStarted};
};

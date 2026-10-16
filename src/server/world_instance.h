#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "event_bus.h"
#include "network_events.h"
#include "thread_safe_queue.h"

// TODO: заменить на настоящую структуру корабля из серверной модели мира.
struct ShipData {};

// TODO: заменить на настоящую структуру снаряда из серверной модели мира.
struct ProjectileData {};

// TODO: заменить на настоящую структуру астероида из серверной модели мира.
struct AsteroidData {};

// TODO: заменить на фабрику, генерирующую астероиды для поддержания популяции на карте.
class AsteroidFactory {};

// TODO: заменить на настоящее событие действия игрока, переданное в ThreadSafeQueue.
struct ActionEvent : public events::Event {};

// TODO: заменить на настоящий тип идентификатора сущностей из общей модели мира (snapshot::EntityId).
using EntityId = std::uint32_t;

// TODO: заменить на настоящий снимок состояния мира (snapshot::GameSnapshot).
struct GameSnapshot {};

/**
 * @brief Единый игровой мир сервера.
 * @details Зона ответственности:
 *          - Хранение состояния мира: корабли, снаряды, астероиды.
 *          - Очередь действий игроков (ThreadSafeQueue<ActionEvent>).
 *          - Генерация идентификаторов сущностей.
 *          - Игровой цикл с фиксированной частотой тиков на базе boost::asio::steady_timer.
 *
 *          Симуляция пока не реализована: все шаги тика — заглушки с TODO.
 *
 * @note Модель потокобезопасности:
 *       - Тики выполняются на потоке(ах), обслуживающем(их) io_context; состояние
 *         мира (ships_ и т.д.) доступно только внутри doTick().
 *       - enqueueAction(), start(), stop(), currentTick(), lastSnapshot()
 *         безопасны для вызова из любых потоков.
 *       - Внешние мутации мира выполняются только через очередь команд и
 *         применяются на ближайшем тике.
 * @note Объект должен жить в std::shared_ptr (см. create()): async-цепочка тиков
 *       удерживает себя через shared_from_this() и гарантирует деинициализацию
 *       только после отмены таймера.
 * @note При работе с NetworkContext перед его stop()/wait() обязательно вызвать
 *       stop() — иначе активный steady_timer не даст io_context::run() завершиться.
 */
class WorldInstance : public std::enable_shared_from_this<WorldInstance> {
public:
    /**
     * @brief Фабрика создания игрового мира.
     * @details Конструктор приватный, т.к. async-цепочка тиков удерживает объект
     *          через shared_from_this() — экземпляр должен жить в shared_ptr.
     * @param io_context Входные данные: Контекст ввода-вывода Asio для таймера тиков.
     * @param event_bus Входные данные: Шина событий сервера для публикации снимков состояния. Не должен быть nullptr.
     * @param tick_rate_hz Входные данные: Частота тиков в Гц (интервал 1000 / tick_rate_hz мс).
     * @throws std::invalid_argument Если tick_rate_hz меньше либо равен 0.
     * @return std::shared_ptr<WorldInstance> Указатель на созданный игровой мир.
     */
    static std::shared_ptr<WorldInstance> create(boost::asio::io_context& io_context,
                                                 std::shared_ptr<events::EventBus> event_bus,
                                                 double tick_rate_hz = kDefaultTickRateHz);

    /**
     * @brief Деструктор игрового мира.
     * @details Останавливает игровой цикл и отменяет активный таймер.
     */
    ~WorldInstance();

    WorldInstance(const WorldInstance&) = delete;
    WorldInstance& operator=(const WorldInstance&) = delete;
    WorldInstance(WorldInstance&&) = delete;
    WorldInstance& operator=(WorldInstance&&) = delete;

    /**
     * @brief Запускает игровой цикл тиков.
     * @details Планирует первый тик через tick_timer_. Повторный вызов — no-op.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void start();

    /**
     * @brief Останавливает игровой цикл тиков.
     * @details Снимает флаг is_running_ и отменяет tick_timer_. Повторный вызов — no-op.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void stop();

    /**
     * @brief Потокобезопасно ставит действие игрока в очередь обработки.
     * @param action Входные данные: Действие игрока (ROTATE, THRUST, SHOOT и т.д.).
     * @outputs Выходных значений нет.
     */
    void enqueueAction(ActionEvent action);

    /**
     * @brief Возвращает номер последнего завершённого тика.
     * @details Потокобезопасен: читает атомарный счётчик.
     * @return std::uint64_t Номер текущего тика.
     */
    [[nodiscard]] std::uint64_t currentTick() const noexcept;

    /**
     * @brief Возвращает последний опубликованный снимок состояния мира.
     * @details Потокобезопасен: возвращает копию снимка под last_snapshot_mutex_.
     * @return std::shared_ptr<const GameSnapshot> Снимок состояния или nullptr,
     *         если тики ещё не запускались.
     */
    [[nodiscard]] std::shared_ptr<const GameSnapshot> lastSnapshot() const noexcept;

private:
    /**
     * @brief Конструктор игрового мира.
     * @details Приватный: создание только через WorldInstance::create().
     * @param io_context Входные данные: Контекст ввода-вывода Asio для таймера тиков.
     * @param event_bus Входные данные: Шина событий сервера для публикации снимков состояния. Не должен быть nullptr.
     * @param tick_rate_hz Входные данные: Частота тиков в Гц (интервал 1000 / tick_rate_hz мс).
     * @throws std::invalid_argument Если tick_rate_hz меньше либо равен 0.
     */
    WorldInstance(boost::asio::io_context& io_context,
                  std::shared_ptr<events::EventBus> event_bus,
                  double tick_rate_hz = kDefaultTickRateHz);

    /**
     * @brief Выполняет один шаг игрового тика и планирует следующий.
     * @details При ошибке operation_aborted (отмена таймера stop()) тик пропускается.
     *          Прочие ошибки таймера логируются, и игровой цикл останавливается.
     *          Исключения из шагов тика перехватываются и логируются, цикл продолжается.
     * @param error Входные данные: Код ошибки Asio по результату ожидания таймера.
     * @outputs Выходных значений нет.
     */
    void doTick(const boost::system::error_code& error);

    /**
     * @brief Обрабатывает накопленные действия игроков.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void processInputQueue();

    /**
     * @brief Обновляет физику мира (движение кораблей, снарядов, астероидов).
     * @param dt Входные данные: Время, прошедшее с предыдущего тика, в секундах.
     * @outputs Выходных значений нет.
     */
    void updatePhysics(float dt);

    /**
     * @brief Рассчитывает коллизии между сущностями мира.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void resolveCollisions();

    /**
     * @brief Поддерживает популяцию астероидов через asteroid_factory_.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void maintainAsteroidPopulation();

    /**
     * @brief Собирает снимок текущего состояния мира для рассылки.
     * @return GameSnapshot Снимок состояния мира.
     */
    [[nodiscard]] GameSnapshot buildGameSnapshot();

    /**
     * @brief Рассылает снимок состояния мира всем клиентам через event_bus_.
     * @details Заглушка: на данном этапе не реализовано, см. TODO.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void broadcastSnapshot();

    // TODO: реализовать функции обработки коллизий, когда появится модель сущностей.
    // void onCollision(ShipData& ship1, ShipData& ship2);
    // void onCollision(ShipData& ship, AsteroidData& asteroid);
    // void onCollision(ProjectileData& proj, ShipData& ship);
    // void onCollision(ProjectileData& proj, AsteroidData& asteroid);

    static constexpr double kDefaultTickRateHz = 60.0;  ///< Частота тиков по умолчанию (60 FPS)

    std::atomic<std::uint64_t> current_tick_{0};  ///< Номер текущего тика
    std::atomic<std::uint64_t> next_entity_id_{1};  ///< Генератор идентификаторов сущностей
    ThreadSafeQueue<ActionEvent> action_queue_;                 ///< Очередь действий игроков
    std::unordered_map<SessionId, ShipData> ships_;             ///< Корабли игроков по SessionId
    std::unordered_map<EntityId, ProjectileData> projectiles_;  ///< Снаряды по EntityId
    std::unordered_map<EntityId, AsteroidData> asteroids_;      ///< Астероиды по EntityId
    [[maybe_unused]] AsteroidFactory asteroid_factory_;         ///< Фабрика астероидов
    std::shared_ptr<events::EventBus> event_bus_;               ///< Шина событий сервера
    boost::asio::steady_timer tick_timer_;                      ///< Таймер игровых тиков
    std::chrono::milliseconds tick_interval_;                   ///< Интервал одного тика
    std::atomic<bool> is_running_{false};                       ///< Флаг работы игрового цикла
    std::shared_ptr<const GameSnapshot> last_snapshot_;  ///< Последний снимок мира для внешних читателей
    mutable std::mutex last_snapshot_mutex_;  ///< Защита last_snapshot_
};

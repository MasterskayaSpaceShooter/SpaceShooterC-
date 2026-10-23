#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "config.h"
#include "entity_pool.h"
#include "event_bus.h"
#include "game_snapshot.h"
#include "math_utils.h"
#include "network_events.h"
#include "thread_safe_queue.h"

enum class ActionType { PlayerJoin, PlayerQuit, MoveAndShoot, Respawn };

struct ActionEvent {
    SessionId session_id{};
    ActionType type{ActionType::PlayerJoin};
    math::Vector2D move_direction{};
    math::Vector2D target_point{};
    bool is_shooting{false};
    std::string player_name{};
};

/**
 * @brief Единый игровой мир сервера.
 * @details Зона ответственности:
 *          - Хранение состояния мира: корабли, снаряды, астероиды
 *            в едином пуле domain::EntityPool entities_.
 *          - Очередь действий игроков (ThreadSafeQueue<ActionEvent>).
 *          - Игровой цикл с фиксированной частотой тиков на базе boost::asio::steady_timer.
 *
 *          Симуляция пока не реализована: все шаги тика — заглушки с TODO.
 *
 * @note Модель потокобезопасности:
 *       - Все операции игрового цикла (запуск, остановка, планирование тиков,
 *         выполнение шагов тика) сериализованы через tick_strand_ и исполняются
 *         на потоке(ах), обслуживающем(их) io_context; состояние мира (entities_)
 *         доступно только внутри doTick(). start()/stop() лишь помещают работу
 *         на strand и возвращаются немедленно.
 *       - enqueueAction(), currentTick(), lastSnapshot() безопасны для вызова
 *         из любых потоков.
 *       - Внешние мутации мира выполняются только через очередь команд и
 *         применяются на ближайшем тике.
 * @note Объект должен жить в std::shared_ptr (см. create()): async-цепочка тиков
 *       удерживает себя через shared_from_this() и гарантирует деинициализацию
 *       только после отмены таймера.
 * @note При работе с NetworkContext перед его stop()/wait() обязательно вызвать
 *       stop() — иначе активный steady_timer не даст io_context::run() завершиться.
 */
class WorldInstance : public std::enable_shared_from_this<WorldInstance> {
private:
    // Маркер доступа для фабрики create() (паттерн Badge/passkey): создавать
    // его может только член класса, поэтому конструктор эффективно приватный.
    struct CreateToken {};

public:
    /**
     * @brief Фабрика создания игрового мира.
     * @details Единственный способ создать WorldInstance: доступ к приватному
     *          маркеру CreateToken есть только у членов класса, поэтому
     *          экземпляр всегда живёт в std::shared_ptr (async-цепочка тиков
     *          удерживает себя через shared_from_this()).
     * @param io_context Входные данные: Контекст ввода-вывода Asio для таймера тиков.
     * @param event_bus Входные данные: Шина событий сервера для публикации снимков состояния. Не должен быть nullptr.
     * @param tick_rate_hz Входные данные: Частота тиков в Гц (интервал 1000 / tick_rate_hz мс).
     * @param world_width Входные данные: Ширина мира, по умолчанию 2000.0.
     * @param world_height Входные данные: Высота мира, по умолчанию 2000.0.
     * @throws std::invalid_argument Если event_bus равен nullptr, либо tick_rate_hz, world_width или world_height
     * меньше либо равны 0.
     * @return std::shared_ptr<WorldInstance> Указатель на созданный игровой мир.
     * @note io_context должен пережить WorldInstance (outlive): таймер и strand
     *       используют его ресурсы, и обращение к ним после уничтожения
     *       io_context — неопределённое поведение.
     */
    static std::shared_ptr<WorldInstance> create(boost::asio::io_context& io_context,
                                                 std::shared_ptr<events::EventBus> event_bus,
                                                 double tick_rate_hz = world::kDefaultTickRateHz,
                                                 math::Scalar world_width = world::kDefaultWidth,
                                                 math::Scalar world_height = world::kDefaultHeight);

    /**
     * @brief Конструктор игрового мира.
     * @details Паттерн Badge (passkey): конструктор публичный, но требует
     *          приватный маркер CreateToken, создать который может только
     *          член класса. Это позволяет создавать экземпляр через
     *          std::make_shared, сохраняя контроль фабрики create().
     * @param io_context Входные данные: Контекст ввода-вывода Asio для таймера тиков.
     * @param event_bus Входные данные: Шина событий сервера для публикации снимков состояния. Не должен быть nullptr.
     * @param tick_rate_hz Входные данные: Частота тиков в Гц (интервал 1000 / tick_rate_hz мс).
     * @param world_width Входные данные: Ширина мира, по умолчанию 2000.0.
     * @param world_height Входные данные: Высота мира, по умолчанию 2000.0.
     * @param Входные данные: Приватный маркер доступа (паттерн Badge).
     * @throws std::invalid_argument Если event_bus равен nullptr, либо tick_rate_hz, world_width или world_height
     * меньше либо равны 0.
     * @note io_context должен пережить WorldInstance (outlive): таймер и strand
     *       используют его ресурсы, и обращение к ним после уничтожения
     *       io_context — неопределённое поведение.
     */
    WorldInstance(boost::asio::io_context& io_context,
                  std::shared_ptr<events::EventBus> event_bus,
                  double tick_rate_hz,
                  CreateToken,
                  math::Scalar world_width = world::kDefaultWidth,
                  math::Scalar world_height = world::kDefaultHeight);

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
     * @details Fire-and-forget: планирует doStart() на tick_strand_. Первый тик
     *          планируется асинхронно через tick_timer_ (начальная точка каденции
     *          «сейчас + интервал»). Повторный вызов до фактической остановки — no-op.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void start();

    /**
     * @brief Останавливает игровой цикл тиков.
     * @details Fire-and-forget: планирует doStop() на tick_strand_, который
     *          переводит состояние в Stopped и отменяет tick_timer_.
     *          Повторный вызов — no-op.
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
     * @return std::shared_ptr<const snapshot::GameSnapshot> Снимок состояния или nullptr,
     *         если тики ещё не запускались.
     */
    [[nodiscard]] std::shared_ptr<const snapshot::GameSnapshot> lastSnapshot() const noexcept;

    /**
     * @brief Возвращает до count свободных позиций внутри мировых границ.
     * @details Свободная позиция — узел регулярной сетки, не пересекающийся ни с одной
     *          живой сущностью мира (учитываются все типы, включая Bullet).
     *          Реализация отсутствует: появится вместе с симуляцией мира;
     *          границы мира берутся из world_width_/world_height_.
     * @param count Входные данные: Максимальное число позиций (0 -> пустой результат).
     * @param clearance Входные данные: Радиус «запретной» окружности кандидата, по умолчанию 30.0.
     * @return std::vector<math::Vector2D> Свободные позиции (не более count).
     * @note Константен: читает entities_ — вызывать
     *       только в контексте тика (на tick_strand_).
     */
    [[nodiscard]] std::vector<math::Vector2D> freePositions(std::size_t count,
                                                            math::Scalar clearance = world::kDefaultClearance) const;

private:
    /**
     * @brief Состояния жизненного цикла игрового цикла.
     */
    enum class State : std::uint8_t {
        Stopped,  ///< Тики не планируются (исходное состояние и состояние после stop()).
        Running,  ///< Тики планируются и выполняются на потоке io_context.
    };

    /**
     * @brief Выполняет один шаг игрового тика и планирует следующий.
     * @details При ошибке operation_aborted (отмена таймера stop()) тик пропускается.
     *          Прочие ошибки таймера логируются, и игровой цикл останавливается.
     *          Следующий тик планируется до выполнения тела текущего,
     *          чтобы работа тика не сдвигала абсолютную каденцию.
     *          Исключения из шагов тика перехватываются в runTickStep() и логируются,
     *          цикл продолжается. Исключения из самого тела (планирование, шаги тика)
     *          перехватываются здесь, логируются, и цикл переводится в Stopped.
     * @param error Входные данные: Код ошибки Asio по результату ожидания таймера.
     * @outputs Выходных значений нет.
     */
    void doTick(const boost::system::error_code& error);

    /**
     * @brief Выполняет тело одного игрового тика: обработка входов, физика,
     *        коллизии, поддержание популяции астероидов, сбор и рассылка снимка.
     * @details Исключения любого рода перехватываются и логируются, чтобы
     *          одиночный сбой не остановил игровой цикл.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void runTickStep();

    /**
     * @brief Планирует следующий тик через tick_timer_.
     * @details Жёсткая каденция: следующий дедлайн = текущий дедлайн + интервал,
     *          а не «сейчас + интервал». Если дедлайн отстал от реального времени
     *          больше чем на world::kMaxMissedTicks интервалов (работа тика дольше
     *          интервала), каденция переустанавливается относительно now(),
     *          а упущенные тики пропускаются.
     *          No-op, если состояние не Running — это исключает повторное
     *          «оживление» цепочки тиков после stop()/перезапуска.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void scheduleNextTick();

    /**
     * @brief Реализация запуска, исполняется на tick_strand_.
     * @details Переводит состояние Stopped -> Running и засеивает первый тик.
     *          Всё это — единая сериализованная секция с doStop(), поэтому
     *          «cancel до arm» и двойная цепочка тиков исключены.
     *          Исключения логируются, состояние возвращается в Stopped,
     *          чтобы повторный start() мог восстановить цикл.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void doStart();

    /**
     * @brief Реализация остановки, исполняется на tick_strand_.
     * @details Переводит состояние Running -> Stopped и отменяет tick_timer_.
     *          Исключения логируются и проглатываются, чтобы сбой останова
     *          не вылетел из Asio-хендлера.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void doStop();

    /**
     * @brief Отменяет tick_timer_ и выбрасывает накопленную очередь действий.
     * @details Исполняется на tick_strand_ уже после перевода state_ в Stopped:
     *          и при штатной остановке (doStop), и при сбое таймера (doTick).
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void shutdownTickLoop();

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
     * @brief Поддерживает популяцию астероидов, создавая новые в entities_.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void maintainAsteroidPopulation();

    /**
     * @brief Собирает снимок текущего состояния мира для рассылки.
     * @return snapshot::GameSnapshot Снимок состояния мира.
     */
    [[nodiscard]] snapshot::GameSnapshot buildGameSnapshot();

    /**
     * @brief Рассылает снимок состояния мира всем клиентам через event_bus_.
     * @details Заглушка: на данном этапе не реализовано, см. TODO.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void broadcastSnapshot();

    /**
     * @brief Здесь лежат все подписки
     * @details Вызывается из create() и вызывает обработчики подписок
     * @inputs ходных параметров нет.
     * @outputs Выходных значений нет.
     */
    void setupSubscriptions();

    /**
     * @brief Колбек для подписки на событие ClientDisconnectedEvent
     * @details Создаёт ActionType::PlayerQuit и кладёт в очерердь
     * @inputs Входные параметры: событие из шины ClientDisconnectedEvent
     * @outputs Выходных значений нет.
     */
    void onClientDisconnected(const ClientDisconnectedEvent& event);

    // TODO: реализовать функции обработки коллизий на основе domain::Entity.
    // void onCollision(domain::Entity& ship1, domain::Entity& ship2);
    // void onCollision(domain::Entity& ship, domain::Entity& asteroid);
    // void onCollision(domain::Entity& proj, domain::Entity& ship);
    // void onCollision(domain::Entity& proj, domain::Entity& asteroid);

    std::atomic<std::uint64_t> current_tick_{0};    ///< Номер текущего тика
    ThreadSafeQueue<ActionEvent> action_queue_;     ///< Очередь действий игроков
    [[maybe_unused]] domain::EntityPool entities_;  ///< Пул сущностей мира
    std::shared_ptr<events::EventBus> event_bus_;   ///< Шина событий сервера
    boost::asio::strand<boost::asio::io_context::executor_type> tick_strand_;  ///< Сериализация всех операций цикла
    boost::asio::steady_timer tick_timer_;              ///< Таймер игровых тиков
    std::chrono::microseconds tick_interval_;           ///< Интервал одного тика
    math::Scalar world_width_{world::kDefaultWidth};    ///< Ширина мира
    math::Scalar world_height_{world::kDefaultHeight};  ///< Высота мира
    std::atomic<State> state_{State::Stopped};  ///< Состояние жизненного цикла игрового цикла
    std::shared_ptr<const snapshot::GameSnapshot> last_snapshot_;  ///< Последний снимок мира для внешних читателей
    mutable std::mutex last_snapshot_mutex_;             ///< Защита last_snapshot_
    boost::signals2::scoped_connection sub_disconnect_;  ///< Подписка на ClientDisconnectedEvent
};

#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <boost/signals2/connection.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "config.h"
#include "entity_pool.h"
#include "event_bus.h"
#include "game_snapshot.h"
#include "math_utils.h"
#include "network_events.h"
#include "thread_safe_queue.h"
#include "types.h"

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
 *       - enqueueAction(), currentTick() безопасны для вызова
 *         из любых потоков.
 * @note Объект должен жить в std::shared_ptr (см. create()): async-цепочка тиков
 *       удерживает себя через shared_from_this() и гарантирует деинициализацию
 *       только после завершения или отмены таймерного обработчика.
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
     * @param tick_rate_hz Входные данные: Частота тиков в Гц (положительное значение).
     * @param world_width Входные данные: Ширина мира, по умолчанию 2000.0.
     * @param world_height Входные данные: Высота мира, по умолчанию 2000.0.
     * @throws std::invalid_argument Если event_bus равен nullptr, либо tick_rate_hz,
     * world_width или world_height меньше либо равны 0.
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
     * @param tick_rate_hz Входные данные: Частота тиков в Гц (положительное значение).
     * @param world_width Входные данные: Ширина мира, по умолчанию 2000.0.
     * @param world_height Входные данные: Высота мира, по умолчанию 2000.0.
     * @param Входные данные: Приватный маркер доступа (паттерн Badge).
     * @throws std::invalid_argument Если event_bus равен nullptr, либо tick_rate_hz,
     * world_width или world_height меньше либо равны 0.
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
     * @brief Возвращает наибольшую свободную окружность игрового поля.
     * @details Свободная окружность — окружность, целиком лежащая внутри мировых границ
     *          и не пересекающаяся и не касающаяся ни одной живой сущности (учитываются
     *          все типы, включая Bullet). Окружность с максимальным радиусом; при равенстве
     *          радиусов выбирается ближайшая к началу координат.
     * @return math::Circle Наибольшая свободная окружность. Если живых сущностей нет —
     *         вписанная в мир окружность с центром в (0,0).
     * @note Константен: читает entities_ — вызывать только в контексте тика (на tick_strand_).
     */
    [[nodiscard]] math::Circle freePosition() const;

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
     *          Исключения любого этапа приводят к остановке цикла; номер тика
     *          увеличивается только после успешного выполнения всех этапов.
     * @param error Входные данные: Код ошибки Asio по результату ожидания таймера.
     * @outputs Выходных значений нет.
     */
    void doTick(const boost::system::error_code& error);

    /**
     * @brief Выполняет тело одного игрового тика: обработка входов, физика,
     *        коллизии, поддержание популяции астероидов, спавн кораблей новых
     *        игроков, сбор и рассылка снимка.
     * @details Порядок шагов:
     *          1. processInputQueue() — накопление действий игроков (в т.ч. PlayerJoin).
     *          2. updatePhysics(), resolveCollisions(), maintainAsteroidPopulation() —
     *             расчёт мира за тик.
     *          3. spawnShips() — создание кораблей новых игроков ПОСЛЕ физики,
     *             поэтому они не участвуют в расчётах текущего тика.
     *          4. buildGameSnapshot() + broadcastSnapshot().
     *             Приветственный снимок (с кораблями новичков) доставляет новым
     *             игрокам broadcast: их сессии уже в SessionRegistry к моменту
     *             рассылки, поэтому отдельная персональная рассылка не нужна.
     * @details Исключения любого рода перехватываются и логируются, чтобы
     *          одиночный сбой не остановил игровой цикл.
     *        коллизии, поддержание популяции астероидов, сбор и рассылка снимка.
     * @details Исключения передаются в doTick(), чтобы частичный тик не считался
     *          завершённым.
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
     *          Исключения логируются и проглатываются.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void doStop();

    /**
     * @brief Отменяет tick_timer_ и выбрасывает накопленную очередь действий.
     * @details Вызывается после перевода state_ в Stopped: при штатной остановке
     *          (doStop) и при сбое таймера (doTick).
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void shutdownTickLoop();

    // Сессия нового игрока и его имя; корабль создаётся после расчёта физики
    // мира (см. spawnShips() и runTickStep()).
    struct PendingJoin {
        SessionId session_id;
        std::string player_name;
    };

    /**
     * @brief Обрабатывает накопленные действия игроков.
     * @details Выполняется в начале тика. Корабль нового игрока создаётся не здесь,
     *          а позже в spawnShips() (после расчёта физики); здесь лишь накапливается
     *          session_id и имя игрока для последующего спавна (см. runTickStep()).
     * @param new_players Входные данные: Буфер для новых игроков текущего тика.
     * @outputs Выходных значений нет.
     */
    void processInputQueue(std::vector<PendingJoin>& new_players);

    /**
     * @brief Обновляет физику мира (движение кораблей, снарядов, астероидов).
     * @param dt Входные данные: Время, прошедшее с предыдущего тика, в секундах.
     * @outputs Выходных значений нет.
     */
    void updatePhysics(math::Scalar dt);

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
     * @brief Создаёт корабли новых игроков в свободных от объектов областях мира.
     * @details Вызывается в конце тика ПОСЛЕ расчёта физики и коллизий: корабль
     *          появляется в центре наибольшей свободной окружности (freePosition())
     *          и попадает в снимок текущего тика, но в физике этого тика не участвует.
     * @param new_players Входные данные: Сессии новых игроков текущего тика.
     * @outputs Выходных значений нет.
     */
    void spawnShips(const std::vector<PendingJoin>& new_players);

    /**
     * @brief Собирает снимок текущего состояния мира для рассылки.
     * @details Заполняет переиспользуемый буфер snapshot_buffer_ снимками живых
     *          сущностей, чтобы не аллоцировать вектор на каждом тике.
     * @return snapshot::GameSnapshot Снимок состояния мира.
     */
    [[nodiscard]] snapshot::GameSnapshot buildGameSnapshot();

    /**
     * @brief Рассылает снимок состояния мира всем клиентам через event_bus_.
     * @details Принимает готовый снимок из runTickStep(), чтобы не собирать
     *          его повторно внутри рассылки.
     * @param snapshot Входные данные: Готовый снимок состояния мира.
     * @outputs Выходных значений нет.
     */
    void broadcastSnapshot(const snapshot::GameSnapshot& snapshot);

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

    // TODO: реализовать остальные обработчики коллизий.
    void onCollisionShipShip(domain::Entity& ship1, domain::Entity& ship2);
    void onCollision(domain::Entity& ship, domain::Entity& asteroid);
    // void onCollision(domain::Entity& proj, domain::Entity& ship);
    // void onCollision(domain::Entity& proj, domain::Entity& asteroid);

    std::atomic<std::uint64_t> current_tick_{0};    ///< Номер текущего тика
    ThreadSafeQueue<ActionEvent> action_queue_;     ///< Очередь действий игроков
    [[maybe_unused]] domain::EntityPool entities_;  ///< Пул сущностей мира
    std::vector<snapshot::EntitySnapshot> snapshot_buffer_;  ///< Переиспользуемый буфер снимков между тиками
    std::shared_ptr<events::EventBus> event_bus_;  ///< Шина событий сервера
    boost::asio::strand<boost::asio::io_context::executor_type> tick_strand_;  ///< Сериализация всех операций цикла
    boost::asio::steady_timer tick_timer_;     ///< Таймер игровых тиков
    std::chrono::microseconds tick_interval_;  ///< Интервал одного тика
    mutable std::mutex action_mutex_;          ///< Синхронизация enqueueAction() с остановкой
    math::Scalar world_width_{world::kDefaultWidth};    ///< Ширина мира
    math::Scalar world_height_{world::kDefaultHeight};  ///< Высота мира
    std::atomic<State> state_{State::Stopped};  ///< Состояние жизненного цикла игрового цикла
    boost::signals2::scoped_connection sub_disconnect_;               ///< Подписка на ClientDisconnectedEvent
    std::unordered_map<SessionId, types::EntityId> session_to_ship_;  ///< связь сессии и корябля
};

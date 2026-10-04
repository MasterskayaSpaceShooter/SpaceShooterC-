#pragma once

#include <format>
#include <functional>
#include <limits>
#include <memory>
#include <shared_mutex>
#include <utility>
#include <vector>

#include "event_bus.h"
#include "network_events.h"

/**
 * @brief Маршрутизатор и точка управления ВСЕМИ сетевыми событиями.
 * @details Зона ответственности:
 *          - Перехватывает все сетевые события из EventBus (NetworkMessageEvent, Connected, Disconnected).
 *          - Передает валидные входящие пакеты во внешний MessageHandler (для отправки в игровой ActionQueue).
 *          - Предоставляет абстрактный API отправки (sendTo, broadcast) с помощью SendPacketEvent.
 */
class NetworkResponseRouter : public std::enable_shared_from_this<NetworkResponseRouter> {
public:
    /// Маркер массовой рассылки в SendPacketEvent (session_id == BROADCAST_SESSION_ID).
    inline static constexpr SessionId BROADCAST_SESSION_ID = 0;

    /// Сигнатура внешнего слушателя входящих сетевых пакетов
    using MessageHandler = std::function<void(SessionId session_id, std::vector<uint8_t> payload)>;

    /**
     * @brief Фабрика создания маршрутизатора.
     * @details Гарантирует владение маршрутизатором через std::shared_ptr, что необходимо
     *          для безопасной диспетчеризации событий (weak_ptr в подписках) и исключения
     *          висячих колбэков при конкурентной публикации. Если поток T1 публикует
     *          NetworkMessageEvent (уже скопировал список слотов и внутри signal(event)
     *          см. event_bus.h, а поток T2 разрушает роутер — лямбда всё равно выполнится и
     *          разыменует this уже освобождённого объекта. Это гонка → UAF/краш.
     * @param event_bus Входные данные: Указатель на шину событий.
     * @return std::shared_ptr<NetworkResponseRouter> Экземпляр маршрутизатора.
     */
    static std::shared_ptr<NetworkResponseRouter> create(std::shared_ptr<events::EventBus> event_bus);

    /**
     * @brief Деструктор маршрутизатора.
     * @details Отключает подписки на события шины (scoped_connection делает это автоматически).
     */
    ~NetworkResponseRouter();

    /**
     * @brief Регистрирует внешний обработчик входящих декодированных кадров.
     * @details Взаимодействует с полями: message_handler_, message_handler_mutex_.
     *          Запись выполняется под эксклюзивной блокировкой для защиты от гонок
     *          с одновременным чтением в onMessageReceived().
     * @param handler Входные данные: Функция-колбэк вида void(SessionId, vector<uint8_t>).
     * @outputs Выходных значений нет.
     */
    void setMessageHandler(MessageHandler handler);

    /**
     * @brief Формирует и публикует событие отправки пакета конкретному клиенту.
     * @details Взаимодействует с полем: event_bus_. Генерирует SendPacketEvent.
     * @param session_id Входные данные: ID целевого клиента.
     * @param payload Входные данные: Массив байт кадра.
     * @outputs Выходных значений нет.
     */
    void sendTo(SessionId session_id, std::vector<uint8_t> payload);

    /**
     * @brief Формирует и публикует событие массовой рассылки пакета всем клиентам.
     * @details Взаимодействует с полем: event_bus_. Генерирует SendPacketEvent с session_id = 0.
     * @param payload Входные данные: Массив байт кадра.
     * @outputs Выходных значений нет.
     */
    void broadcast(std::vector<uint8_t> payload);

private:
    /**
     * @brief Приватный конструктор маршрутизатора.
     * @details Взаимодействует с полем: event_bus_. Вызывает setupSubscriptions().
     *          Создание доступно только через фабрику create().
     * @param event_bus Входные данные: Указатель на шину событий.
     */
    explicit NetworkResponseRouter(std::shared_ptr<events::EventBus> event_bus);

    /**
     * @brief Подписывает методы класса на события NetworkMessageEvent, ClientConnectedEvent, ClientDisconnectedEvent в
     * EventBus.
     * @details Взаимодействует с полем: event_bus_.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void setupSubscriptions();

    /**
     * @brief Внутренний обработчик прихода входящего кадра от клиента.
     * @details Взаимодействует с полями: message_handler_, message_handler_mutex_.
     *          Копирует обработчик под разделяемой блокировкой и вызывает его вне блокировки,
     *          чтобы избежать долгого удержания мьютекса и дедлоков.
     * @param event Входные данные: Структура события NetworkMessageEvent.
     * @outputs Выходных значений нет.
     */
    void onMessageReceived(const NetworkMessageEvent& event);

    /**
     * @brief Внутренний обработчик события подключения нового клиента.
     * @details Логирует событие подключения.
     * @param event Входные данные: Структура события ClientConnectedEvent.
     * @outputs Выходных значений нет.
     */
    void onClientConnected(const ClientConnectedEvent& event);

    /**
     * @brief Внутренний обработчик события отключения клиента.
     * @details Логирует событие отключения.
     * @param event Входные данные: Структура события ClientDisconnectedEvent.
     * @outputs Выходных значений нет.
     */
    void onClientDisconnected(const ClientDisconnectedEvent& event);

    std::shared_ptr<events::EventBus> event_bus_;  ///< Владение шиной событий сервера
    mutable std::shared_mutex message_handler_mutex_;  ///< Защита message_handler_ от гонок чтения/записи
    MessageHandler message_handler_;  ///< Колбэк передатчик пакетов во внешние системы
    std::vector<boost::signals2::scoped_connection> subscriptions_;  ///< Активные подписки на события шины
};

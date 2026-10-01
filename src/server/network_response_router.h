#pragma once
#include <functional>
#include <limits>
#include <memory>
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
class NetworkResponseRouter {
public:
    /// Сигнатура внешнего слушателя входящих сетевых пакетов
    using MessageHandler = std::function<void(SessionId session_id, std::vector<uint8_t> payload)>;

    /**
     * @brief Конструктор маршрутизатора.
     * @details Взаимодействует с полем: event_bus_. Вызывает setupSubscriptions().
     * @param event_bus Входные данные: Ссылка на шину событий.
     */
<<<<<<< HEAD
<<<<<<< HEAD
    explicit NetworkResponseRouter(events::EventBus& event_bus);
=======
    explicit NetworkResponseRouter(EventBus& event_bus) {};
>>>>>>> e9d2eab (S-22 network-response-router: add methods stubs (#44))
=======
    explicit NetworkResponseRouter(events::EventBus& event_bus);
>>>>>>> cc97926 (feat: add implement NetworkResponseRouter constructor/destructor and associated tests)

    /**
     * @brief Деструктор маршрутизатора.
     * @details Освобождает подписки и ресурсы.
     */
<<<<<<< HEAD
    ~NetworkResponseRouter() {};
=======
    ~NetworkResponseRouter();
>>>>>>> cc97926 (feat: add implement NetworkResponseRouter constructor/destructor and associated tests)

    /**
     * @brief Регистрирует внешний обработчик входящих декодированных кадров.
     * @details Взаимодействует с полем: message_handler_.
     * @param handler Входные данные: Функция-колбэк вида void(SessionId, vector<uint8_t>).
     * @outputs Выходных значений нет.
     */
<<<<<<< HEAD
    void setMessageHandler(MessageHandler handler) {};
=======
    void setMessageHandler(MessageHandler handler);
>>>>>>> cc97926 (feat: add implement NetworkResponseRouter constructor/destructor and associated tests)

    /**
     * @brief Формирует и публикует событие отправки пакета конкретному клиенту.
     * @details Взаимодействует с полем: event_bus_. Генерирует SendPacketEvent.
     * @param session_id Входные данные: ID целевого клиента.
     * @param payload Входные данные: Массив байт кадра.
     * @outputs Выходных значений нет.
     */
    void sendTo(SessionId /*session_id*/, std::vector<uint8_t> /*payload*/){};

    /**
     * @brief Формирует и публикует событие массовой рассылки пакета всем клиентам.
     * @details Взаимодействует с полем: event_bus_. Генерирует SendPacketEvent с session_id = 0.
     * @param payload Входные данные: Массив байт кадра.
     * @outputs Выходных значений нет.
     */
    void broadcast(std::vector<uint8_t> /*payload*/){};

private:
    /**
     * @brief Подписывает методы класса на события NetworkMessageEvent, ClientConnectedEvent, ClientDisconnectedEvent в
     * EventBus.
     * @details Взаимодействует с полем: event_bus_.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
<<<<<<< HEAD
    void setupSubscriptions() {};
=======
    void setupSubscriptions();
>>>>>>> cc97926 (feat: add implement NetworkResponseRouter constructor/destructor and associated tests)

    /**
     * @brief Внутренний обработчик прихода входящего кадра от клиента.
     * @details Взаимодействует с полем: message_handler_. Вызывает зарегистрированный колбэк.
     * @param event Входные данные: Структура события NetworkMessageEvent.
     * @outputs Выходных значений нет.
     */
<<<<<<< HEAD
    void onMessageReceived(const NetworkMessageEvent& event) {};
=======
    void onMessageReceived(const NetworkMessageEvent& event);
>>>>>>> cc97926 (feat: add implement NetworkResponseRouter constructor/destructor and associated tests)

    /**
     * @brief Внутренний обработчик события подключения нового клиента.
     * @details Логирует событие подключения.
     * @param event Входные данные: Структура события ClientConnectedEvent.
     * @outputs Выходных значений нет.
     */
    void onClientConnected(const ClientConnectedEvent& /*event*/) {};

    /**
     * @brief Внутренний обработчик события отключения клиента.
     * @details Логирует событие отключения.
     * @param event Входные данные: Структура события ClientDisconnectedEvent.
     * @outputs Выходных значений нет.
     */
    void onClientDisconnected(const ClientDisconnectedEvent& /*event*/) {};

    events::EventBus& event_bus_;                             ///< Шина событий
    MessageHandler message_handler_;                          ///< Колбэк передатчик пакетов во внешние системы
    std::vector<boost::signals2::connection> subscriptions_;  ///< Активные подписки на события шины
};

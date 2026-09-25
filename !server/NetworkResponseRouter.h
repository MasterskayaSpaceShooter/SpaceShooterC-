#pragma once
#include <functional>
#include <memory>
#include <vector>

#include "NetworkEvents.h"

class EventBus;

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
    explicit NetworkResponseRouter(EventBus& event_bus);

    /**
     * @brief Деструктор маршрутизатора.
     * @details Освобождает подписки и ресурсы.
     */
    ~NetworkResponseRouter();

    /**
     * @brief Регистрирует внешний обработчик входящих декодированных кадров.
     * @details Взаимодействует с полем: message_handler_.
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
     * @brief Подписывает методы класса на события NetworkMessageEvent, ClientConnectedEvent, ClientDisconnectedEvent в
     * EventBus.
     * @details Взаимодействует с полем: event_bus_.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void setupSubscriptions();

    /**
     * @brief Внутренний обработчик прихода входящего кадра от клиента.
     * @details Взаимодействует с полем: message_handler_. Вызывает зарегистрированный колбэк.
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

    EventBus& event_bus_;             ///< Шина событий
    MessageHandler message_handler_;  ///< Колбэк передатчик пакетов во внешние системы
};
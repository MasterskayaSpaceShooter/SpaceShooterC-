#pragma once
#include <unordered_map>
#include <memory>
#include <mutex>
#include "Session.hpp"

/**
 * @brief Потокобезопасный реестр активных клиентских сессий.
 * @details Зона ответственности:
 *          - Хранение владения (`std::shared_ptr<Session>`) всех активных соединений.
 *          - Перенаправление SendPacketEvent из EventBus в нужный объект Session.
 *          - Выполнение массовой рассылки (Broadcast) пакетов при session_id = 0.
 */
class SessionRegistry {
public:
    /**
     * @brief Конструктор реестра сессий.
     * @details Взаимодействует с полем: event_bus_. Подписывается на SendPacketEvent.
     * @param event_bus Входные данные: Ссылка на центральную шину событий сервера.
     */
    explicit SessionRegistry(EventBus& event_bus);

    /**
     * @brief Регистрирует новую созданную сессию в реестре.
     * @details Взаимодействует с полями: sessions_, registry_mutex_.
     * @param session Входные данные: Указатель shared_ptr на объект сессии.
     * @outputs Выходных значений нет.
     */
    void addSession(std::shared_ptr<Session> session);

    /**
     * @brief Удаляет сессию из реестра по ее идентификатору.
     * @details Взаимодействует с полями: sessions_, registry_mutex_.
     * @param id Входные данные: SessionId удаляемого клиента.
     * @outputs Выходных значений нет.
     */
    void removeSession(SessionId id);

    /**
     * @brief Находит и возвращает указатель на сессию по ее ID.
     * @details Взаимодействует с полями: sessions_, registry_mutex_.
     * @param id Входные данные: SessionId искомого клиента.
     * @return std::shared_ptr<Session> Указатель на сессию или nullptr, если клиент не найден.
     */
    std::shared_ptr<Session> getSession(SessionId id);

    /**
     * @brief Отправляет пакет данных абсолютно всем подключенным в данный момент клиентам.
     * @details Взаимодействует с полями: sessions_, registry_mutex_, а также вызывает Session::send().
     * @param data Входные данные: Константная ссылка на вектор байт кадра.
     * @outputs Выходных значений нет.
     */
    void broadcast(const std::vector<uint8_t>& data);

private:
    EventBus& event_bus_;                                               ///< Шина событий сервера
    std::mutex registry_mutex_;                                         ///< Мьютекс защиты таблицы сессий
    std::unordered_map<SessionId, std::shared_ptr<Session>> sessions_;  ///< Карта активных сессий
};

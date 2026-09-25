#pragma once
#include <cstdint>
#include <string>
#include <vector>

/// Уникальный идентификатор сетевого соединения (сессии)
using SessionId = uint64_t;

/**
 * @brief Абстрактное базовое сетевое событие.
 * @details Зона ответственности: Единый базовый тип для всех сетевых уведомлений в EventBus.
 */
struct NetworkEvent {
    SessionId session_id{0};  ///< ID сессии (0 используется как маркер для Broadcast)

    /**
     * @brief Виртуальный деструктор для корректного полиморфного удаления.
     */
    virtual ~NetworkEvent() = default;

protected:
    /**
     * @brief Защищенный конструктор базового события.
     * @param id Входные данные: Идентификатор сетевой сессии.
     */
    explicit NetworkEvent(SessionId id) : session_id(id) {}
};

/**
 * @brief Событие: От клиента получены входящие декодированные данные.
 * @details Генерируется классом Session после нарезки кадров с помощью FrameCodec.
 */
struct NetworkMessageEvent : public NetworkEvent {
    std::vector<uint8_t> payload;  ///< Сырой массив байт полученного кадра

    /**
     * @brief Конструктор события входящего сообщения.
     * @param id Входные данные: ID сессии-отправителя.
     * @param data Входные данные: Вектор байт полезной нагрузки кадра.
     */
    NetworkMessageEvent(SessionId id, std::vector<uint8_t> data) : NetworkEvent(id), payload(std::move(data)) {}
};

/**
 * @brief Событие: Запрос на отправку данных клиенту (или всем клиентам).
 * @details Генерируется NetworkResponseRouter для отправки через SessionRegistry.
 */
struct SendPacketEvent : public NetworkEvent {
    std::vector<uint8_t> payload;  ///< Данные для кодирования и отправки

    /**
     * @brief Конструктор события отправки пакета.
     * @param id Входные данные: ID целевой сессии (0 для Broadcast).
     * @param data Входные данные: Вектор байт для отправки.
     */
    SendPacketEvent(SessionId id, std::vector<uint8_t> data) : NetworkEvent(id), payload(std::move(data)) {}
};

/**
 * @brief Событие: Новый клиент установил TCP-соединение.
 * @details Генерируется классом TcpServer.
 */
struct ClientConnectedEvent : public NetworkEvent {
    std::string remote_address;  ///< Сетевой адрес клиента в формате "IP:port"

    /**
     * @brief Конструктор события подключения нового клиента.
     * @param id Входные данные: Сгенерированный уникальный ID новой сессии.
     * @param address Входные данные: Строковый адрес клиента (IP:Port).
     */
    ClientConnectedEvent(SessionId id, std::string address) : NetworkEvent(id), remote_address(std::move(address)) {}
};

/**
 * @brief Событие: Клиент разорвал соединение или был отключен по ошибке.
 * @details Генерируется классом Session при ошибке I/O или закрытии сокета.
 */
struct ClientDisconnectedEvent : public NetworkEvent {
    /**
     * @brief Конструктор события отключения клиента.
     * @param id Входные данные: ID закрытой сессии.
     */
    explicit ClientDisconnectedEvent(SessionId id) : NetworkEvent(id) {}
};
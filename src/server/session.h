#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include "NetworkEvents.h"

class FrameCodec;
class EventBus;

/**
 * @brief Класс асинхронной сетевой сессии подключенного клиента.
 * @details Зона ответственности:
 *          - Управление TCP-сокетом конкретного соединения.
 *          - Выполнение асинхронного чтения/записи через Boost.Asio.
 *          - Нарезка входящего потока байт на кадры с помощью FrameCodec.
 *          - Публикация входящих кадров в EventBus (как NetworkMessageEvent).
 *          - Потокобезопасная очередь отправки для исключения параллельных async_write.
 */
class Session : public std::enable_shared_from_this<Session> {
public:
    /**
     * @brief Конструктор асинхронной сессии.
     * @details Взаимодействует с членами класса: инициализирует socket_, id_, event_bus_, codec_, read_buffer_.
     * @param socket Входные данные: Перемещенный TCP-сокет от акцептора.
     * @param id Входные данные: Уникальный идентификатор сессии (SessionId).
     * @param event_bus Входные данные: Ссылка на шину событий для публикации кадров и дисконнектов.
     * @param codec Входные данные: Ссылка на кодек нарезки кадров.
     */
    Session(boost::asio::ip::tcp::socket socket, SessionId id, EventBus& event_bus, FrameCodec& codec);

    /**
     * @brief Деструктор сессии.
     * @details Взаимодействует с полем: socket_. Автоматически закрывает сокет при уничтожении.
     */
    ~Session();

    /**
     * @brief Запускает асинхронный цикл чтения из сокета.
     * @details Взаимодействует с полем: socket_, read_buffer_. Вызывает приватный метод doRead().
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void start();

    /**
     * @brief Потокобезопасно ставит кадр в очередь отправки клиенту.
     * @details Взаимодействует с полями: write_queue_, write_mutex_, is_writing_.
     *          Запускает doWrite(), если в момент вызова отправка не идет.
     * @param data Входные данные: Массив байт отправляемого кадра.
     * @outputs Выходных значений нет.
     */
    void send(std::vector<uint8_t> data);

    /**
     * @brief Принудительно закрывает сокет сессии и оповещает системы об отключении.
     * @details Взаимодействует с полями: socket_, event_bus_. Публикует ClientDisconnectedEvent.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void close();

    /**
     * @brief Возвращает уникальный идентификатор текущей сессии.
     * @details Взаимодействует с полем: id_.
     * @inputs Входных параметров нет.
     * @return SessionId Числовой идентификатор сессии.
     */
    [[nodiscard]] SessionId getId() const noexcept {
        return id_;
    }

private:
    /**
     * @brief Запускает асинхронную операцию чтения байт из сокета (async_read_some).
     * @details Взаимодействует с полями: socket_, read_buffer_, codec_, event_bus_, id_.
     *          При получении байт передает их в codec_ и публикует извлеченные кадры в event_bus_.
     * @inputs Входных параметров нет (работает через асинхронный колбэк Asio).
     * @outputs Выходных значений нет.
     */
    void doRead();

    /**
     * @brief Запускает асинхронную операцию записи кадра из очереди в сокет (async_write).
     * @details Взаимодействует с полями: socket_, write_queue_, write_mutex_, is_writing_, codec_.
     *          Извлекает кадр из write_queue_, кодирует через codec_ и передает Asio.
     * @inputs Входных параметров нет (работает через асинхронный колбэк Asio).
     * @outputs Выходных значений нет.
     */
    void doWrite();

    boost::asio::ip::tcp::socket socket_;  ///< TCP-сокет подключения
    const SessionId id_;                   ///< Уникальный ID сессии
    EventBus& event_bus_;                  ///< Шина событий сервера
    FrameCodec& codec_;                    ///< Кодек протокола

    std::vector<uint8_t> read_buffer_;               ///< Буфер асинхронного чтения байт
    static constexpr size_t READ_BLOCK_SIZE = 4096;  ///< Размер блока чтения

    std::mutex write_mutex_;                        ///< Мьютекс защиты очереди записи
    std::queue<std::vector<uint8_t>> write_queue_;  ///< Очередь исходящих кадров
    bool is_writing_{false};                        ///< Флаг активности операции async_write
};
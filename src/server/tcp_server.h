#pragma once
#include <atomic>
#include <boost/asio.hpp>
#include <memory>

#include "session_registry.h"

namespace network {
class FrameCodec;
}

/**
 * @brief Приемник входящих TCP-подключений.
 * @details Зона ответственности:
 *          - Прослушивание сетевого порта через `boost::asio::ip::tcp::acceptor`.
 *          - Асинхронный прием клиентов (`async_accept`).
 *          - Генерация уникальных SessionId и создание объектов Session.
 *          - Публикация ClientConnectedEvent в EventBus.
 */
class TcpServer : public std::enable_shared_from_this<TcpServer> {
public:
    /**
     * @brief Конструктор TCP-сервера.
     * @details Взаимодействует с членами класса: инициализирует acceptor_, event_bus_, codec_, session_registry_.
     * @param io_context Входные данные: Контекст ввода-вывода Asio.
     * @param port Входные данные: Сетевой порт для прослушивания.
     * @param event_bus Входные данные: Шина событий сервера. Не должен быть nullptr.
     * @param codec Входные данные: Кодек протокола.
     *
     * @throws std::invalid_argument Если event_bus равен nullptr.
     */
    TcpServer(boost::asio::io_context& io_context,
              uint16_t port,
              std::shared_ptr<events::EventBus> event_bus,
              network::FrameCodec& codec);

    /**
     * @brief Запускает процесс асинхронного прослушивания порта.
     * @details Взаимодействует с полем: acceptor_. Вызывает приватный метод doAccept().
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void start();

    /**
     * @brief Останавливает прием новых подключений.
     * @details Взаимодействует с полем: acceptor_. Закрывает акцептор Asio.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void stop();

private:
    /**
     * @brief Запускает асинхронную операцию ожидания подключения нового клиента (async_accept).
     * @details Взаимодействует с полями: acceptor_, next_session_id_, session_registry_, event_bus_, codec_.
     *          При успешном подключении создает Session, заносит в реестр и публикует событие.
     * @inputs Входных параметров нет (работает через асинхронный колбэк Asio).
     * @outputs Выходных значений нет.
     */
    void doAccept();

    boost::asio::ip::tcp::acceptor acceptor_;      ///< Акцептор TCP-соединений Asio
    std::shared_ptr<events::EventBus> event_bus_;  ///< Шина событий
    network::FrameCodec& codec_;                   ///< Кодек протокола
    SessionRegistry session_registry_;             ///< Реестр сессий клиентов
    std::atomic<SessionId> next_session_id_{1};  ///< Счетчик для генерации уникальных SessionId
};

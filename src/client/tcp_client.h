#pragma once
#include <array>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string_view>
#include <vector>

#include "frame_codec.h"
#include "logger.h"

/**
 * @brief Асинхронный сетевой TCP-клиент.
 * @details Зона ответственности:
 *          - Установление подключения к серверу по IP/порту (`async_connect`).
 *          - Асинхронное чтение данных из сокета, декодирование через FrameCodec и вызов message_cb_.
 *          - Потокобезопасная отправка команд на сервер (с защищенной очередью записи).
 *          - Уведомление об обрыве связи через disconnect_cb_.
 */
class TcpClient : public std::enable_shared_from_this<TcpClient> {
public:
    /// Сигнатура колбэка при получении декодированного кадра
    using MessageCallback = std::function<void(const std::vector<uint8_t>&)>;
    /// Сигнатура колбэка при отключении от сервера
    using DisconnectCallback = std::function<void()>;

    /**
     * @brief Конструктор сетевого клиента.
     * @details Взаимодействует с членами класса: инициализирует socket_, read_buffer_.
     * @param io_context Входные данные: Контекст ввода-вывода Asio.
     */
    explicit TcpClient(boost::asio::io_context& io_context);

    /**
     * @brief Деструктор клиента. Закрывает сокет.
     */
    ~TcpClient();

    /**
     * @brief Асинхронно подключается к серверу по сетевому адресу и порту.
     * @details Взаимодействует с полями: socket_, is_connected_. Вызывает doRead() при успехе.
     * @param host Входные данные: IP-адрес или хост сервера (например, "127.0.0.1").
     * @param port Входные данные: Сетевой порт.
     * @param on_connect Входные данные: Колбэк состояния подключения (true = успех).
     * @outputs Выходных значений нет.
     */
    void connect(const std::string& host, uint16_t port, std::function<void(bool)> on_connect);

    /**
     * @brief Потокобезопасно отправляет кадр данных на сервер.
     * @details Payload большего допустимого размера отклоняется и логируется.
     * @param data Входные данные: Вектор байт кадра.
     * @outputs Выходных значений нет.
     */
    void send(std::vector<uint8_t> data);

    /**
     * @brief Принудительно закрывает соединение с сервером.
     * @details Все изменения состояния и операции с сокетом сериализуются на strand_.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void disconnect() noexcept;

    /**
     * @brief Устанавливает колбэк на получение входящих кадров от сервера.
     * @details Взаимодействует с полем: message_cb_.
     * @param cb Входные данные: Функция вида void(const vector<uint8_t>&).
     * @outputs Выходных значений нет.
     */
    void setMessageCallback(MessageCallback cb) {
        message_cb_ = std::move(cb);
    }

    /**
     * @brief Устанавливает колбэк на событие разрыва соединения.
     * @details Взаимодействует с полем: disconnect_cb_.
     * @param cb Входные данные: Функция вида void().
     * @outputs Выходных значений нет.
     */
    void setDisconnectCallback(DisconnectCallback cb) {
        disconnect_cb_ = std::move(cb);
    }

    /**
     * @brief Возвращает флаг текущего состояния подключения.
     * @details Взаимодействует с полем: is_connected_.
     * @inputs Входных параметров нет.
     * @return true Если сокет открыт и активен.
     * @return false Если клиент отключен.
     */
    [[nodiscard]] bool isConnected() const noexcept {
        return is_connected_;
    }

private:
    // Логирование и отключение при ошибках в контексте асинхронных операций
    void logAndDisconnect(std::string_view context_name) noexcept;
    void handleWriteSetupError(std::string_view context_name) noexcept;

    /**
     * @brief Асинхронно считывает входящие байты от сервера (async_read_some).
     * @details Взаимодействует с полями: socket_, read_buffer_, message_cb_.
     * @inputs Параметров нет (колбэк Asio).
     * @outputs Выходных значений нет.
     */
    void doRead();

    /**
     * @brief Асинхронно записывает кадр из очереди на сервер (async_write).
     * @details Взаимодействует с полями: socket_, write_queue_, is_writing_.
     * @inputs Параметров нет (колбэк Asio).
     * @outputs Выходных значений нет.
     */
    void doWrite();

    boost::asio::strand<boost::asio::io_context::executor_type>
        strand_;  ///< Стрэнд для последовательного выполнения операций
    boost::asio::ip::tcp::socket socket_;                       ///< TCP-сокет клиента
    std::shared_ptr<boost::asio::ip::tcp::resolver> resolver_;  ///< Resolver текущего подключения
    std::atomic<bool> is_connected_{false};                     ///< Флаг подключения
    std::atomic<bool> disconnect_requested_{false};
    uint64_t disconnect_generation_{0};      ///< Изменяется и читается только на strand_
    bool is_connecting_{false};              ///< Изменяется только на strand_
    uint64_t active_connect_generation_{0};  ///< Поколение подключения на strand_
    std::function<void(bool)> pending_connect_callback_;
    // std::vector<uint8_t> read_buffer_;  ///< Временный буфер чтения
    boost::beast::flat_buffer read_buffer_;
    static constexpr size_t READ_BLOCK_SIZE = 4096;

    std::queue<std::shared_ptr<network::FrameCodec::Frame>> write_queue_;  ///< Очередь кадров на отправку
    bool is_writing_{false};                                               ///< Флаг активной записи

    MessageCallback message_cb_;        ///< Обработчик входящего кадра
    DisconnectCallback disconnect_cb_;  ///< Обработчик отключения
};

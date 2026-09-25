#pragma once
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

class FrameCodec;

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
     * @details Взаимодействует с членами класса: инициализирует socket_, codec_, read_buffer_.
     * @param io_context Входные данные: Контекст ввода-вывода Asio.
     * @param codec Входные данные: Ссылка на кодек протокола.
     */
    TcpClient(boost::asio::io_context& io_context, FrameCodec& codec);

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
     * @details Взаимодействует с полями: write_queue_, write_mutex_, is_writing_, socket_.
     * @param data Входные данные: Вектор байт кадра.
     * @outputs Выходных значений нет.
     */
    void send(std::vector<uint8_t> data);

    /**
     * @brief Принудительно закрывает соединение с сервером.
     * @details Взаимодействует с полями: socket_, is_connected_, disconnect_cb_.
     * @inputs Входных параметров нет.
     * @outputs Выходных значений нет.
     */
    void disconnect();

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
    /**
     * @brief Асинхронно считывает входящие байты от сервера (async_read_some).
     * @details Взаимодействует с полями: socket_, read_buffer_, codec_, message_cb_.
     * @inputs Параметров нет (колбэк Asio).
     * @outputs Выходных значений нет.
     */
    void doRead();

    /**
     * @brief Асинхронно записывает кадр из очереди на сервер (async_write).
     * @details Взаимодействует с полями: socket_, write_queue_, write_mutex_, is_writing_, codec_.
     * @inputs Параметров нет (колбэк Asio).
     * @outputs Выходных значений нет.
     */
    void doWrite();

    boost::asio::ip::tcp::socket socket_;  ///< TCP-сокет клиента
    FrameCodec& codec_;                    ///< Кодек протокола

    bool is_connected_{false};          ///< Флаг подключения
    std::vector<uint8_t> read_buffer_;  ///< Временный буфер чтения
    static constexpr size_t READ_BLOCK_SIZE = 4096;

    std::mutex write_mutex_;                        ///< Мьютекс очереди отправки
    std::queue<std::vector<uint8_t>> write_queue_;  ///< Очередь кадра на отправку
    bool is_writing_{false};                        ///< Флаг активной записи

    MessageCallback message_cb_;        ///< Обработчик входящего кадра
    DisconnectCallback disconnect_cb_;  ///< Обработчик отключения
};
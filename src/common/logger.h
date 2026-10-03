#pragma once

#include <boost/asio.hpp>
#include <chrono>
#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

/**
 * @brief Уровень логирования.
 */
enum class LogLevel { DEBUG, INFO, WARN, ERROR };

/**
 * @brief Структура, содержащая метаданные о месте вызова лога в исходном коде.
 */
struct SourceLocation {
    const char* file{""};      ///< Имя файла (__FILE__)
    int line{0};               ///< Номер строки (__LINE__)
    const char* function{""};  ///< Имя функции (__FUNCTION__)
};

/**
 * @brief Потокобезопасный асинхронный логгер на boost::asio::strand с поддержкой метаданных кода.
 * @details Зона ответственности:
 *          - Сериализация вывода сообщений через strand без блокировки потоков.
 *          - Форматирование лога с указанием модуля, файла, строки и функции вызова.
 *
 *          Strand не является мьютексом: потоки не блокируются, а ставят задачи
 *          в очередь io_context для последовательного выполнения.
 *
 *          Logger — синглтон, живёт до конца программы. this в handler'ах валиден.
 */
class Logger {
public:
    /**
     * @brief Получить единственный экземпляр логгера (Singleton).
     * @return Logger& Ссылка на синглтон.
     */
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    ~Logger() = default;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    /**
     * @brief Инициализирует strand логгера, привязывая его к io_context приложения.
     * @details Потокобезопасен и идемпотентен: повторный вызов не перезаписывает strand_.
     *          Взаимодействует с полем: strand_, init_flag_.
     * @warning ДОЛЖЕН быть вызван до первого log(). Иначе чтение strand_ в log()
     *          происходит без синхронизации → data race (UB).
     *          Обычно вызывается в main() до старта io_context.run().
     * @warning io_context должен жить дольше Logger. Иначе strand_ становится dangling.
     * @param io_context Входные данные: Ссылка на io_context из NetworkContext.
     * @outputs Выходных значений нет.
     */
    void init(boost::asio::io_context& io_context) {
        std::call_once(init_flag_, [this, &io_context]() {
            strand_ = std::make_unique<boost::asio::strand<boost::asio::io_context::executor_type>>(
                boost::asio::make_strand(io_context));
        });
    }

    /**
     * @brief Потокобезопасно ставит задачу печати лога в очередь strand_.
     * @details Взаимодействует с полем: strand_.
     * @note Logger — синглтон на статике, живёт до конца программы. this валиден
     *       на момент выполнения handler'ов. weak_from_this() не применяется —
     *       Logger не управляется shared_ptr.
     * @note strand_ читается без синхронизации. Требуется, чтобы init() был вызван
     *       до первого log() — см. docstring init().
     * @param level Входные данные: Уровень лога (DEBUG, INFO, WARN, ERROR).
     * @param loc Входные данные: Метаданные исходного кода (файл, строка, функция).
     * @param message Входные данные: Отформатированный текст сообщения.
     * @outputs Выходных значений нет.
     */
    void log(LogLevel level, const SourceLocation& loc, const std::string& message) {
        auto thread_id = std::this_thread::get_id();
        auto now = std::chrono::system_clock::now();

        if (strand_) {
            boost::asio::post(*strand_, [this, now, thread_id, level, loc, message]() {
                printToConsole(now, thread_id, level, loc, message);
            });
        } else {
            // Резервный синхронный вывод (если вызвали лог до инициализации Asio)
            printToConsole(now, thread_id, level, loc, message);
        }
    }

private:
    Logger() = default;

    /**
     * @brief Форматирует и выводит лог в консоль.
     * @details Взаимодействует с std::cout / std::cerr.
     * @param timestamp Время создания лога.
     * @param thread_id ID потока вызова.
     * @param level Уровень лога.
     * @param loc Метаданные о файле, строке и функции.
     * @param message Текст сообщения.
     */
    void printToConsole(std::chrono::system_clock::time_point timestamp,
                        std::thread::id thread_id,
                        LogLevel level,
                        const SourceLocation& loc,
                        const std::string& message) {
        // Время: YYYY-MM-DD HH:MM:SS.mmm
        auto time_c = std::chrono::system_clock::to_time_t(timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()) % 1000;

        std::tm tm_buf{};
#if defined(_WIN32)
        localtime_s(&tm_buf, &time_c);
#else
        localtime_r(&time_c, &tm_buf);
#endif

        std::string time_str = std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}",
                                           tm_buf.tm_year + 1900,
                                           tm_buf.tm_mon + 1,
                                           tm_buf.tm_mday,
                                           tm_buf.tm_hour,
                                           tm_buf.tm_min,
                                           tm_buf.tm_sec,
                                           ms.count());

        std::string level_str;
        switch (level) {
            case LogLevel::DEBUG:
                level_str = "DEBUG";
                break;
            case LogLevel::INFO:
                level_str = "INFO ";
                break;
            case LogLevel::WARN:
                level_str = "WARN ";
                break;
            case LogLevel::ERROR:
                level_str = "ERROR";
                break;
        }

        // Извлекаем filename и module без аллокаций через std::string_view.
        // Например: "src/network/Session.cpp" -> filename="Session.cpp", module="Session"
        std::string_view file_view{loc.file};
        auto slash = file_view.find_last_of("/\\");
        std::string_view filename = (slash == std::string_view::npos) ? file_view : file_view.substr(slash + 1);

        auto dot = filename.find_last_of('.');
        std::string_view module = (dot == std::string_view::npos) ? filename : filename.substr(0, dot);

        // Формат лога: [TIME][TH:xxxx][MODULE][FILE:LINE][FUNC][LEVEL] message
        std::string formatted = std::format("[{}][TH:{}][{}][{}:{}][{}()][{}] {}\n",
                                            time_str,
                                            std::format("{}", thread_id),
                                            module,
                                            filename,
                                            loc.line,
                                            loc.function,
                                            level_str,
                                            message);

        if (level == LogLevel::ERROR) {
            std::cerr << formatted << std::flush;
        } else {
            std::cout << formatted << std::flush;
        }
    }

    /**
     * @brief Strand для сериализации задач логирования.
     * @note Используется вместо std::mutex: mutex в синглтоне Logger был бы
     *       глобальным на весь сервер, что запрещено правилами. Strand не
     *       блокирует потоки, а ставит задачи в очередь io_context.
     * @note Инициализируется в init(). До init() — nullptr, тогда log()
     *       работает синхронно (fallback).
     * @warning strand_ читается в log() без синхронизации. Требуется, чтобы
     *          init() был вызван до первого log() — см. docstring init().
     */
    std::unique_ptr<boost::asio::strand<boost::asio::io_context::executor_type>> strand_;

    std::once_flag init_flag_;  ///< Защита от повторной инициализации strand_.
};

// ============================================================================
// МАКРОСЫ С АВТОМАТИЧЕСКИМ ЗАХВАТОМ __FILE__, __LINE__, __FUNCTION__
// ============================================================================

/// Вспомогательный макрос сборки SourceLocation
#define LOG_SOURCE_LOC                   \
    SourceLocation {                     \
        __FILE__, __LINE__, __FUNCTION__ \
    }

/// Логирование уровня INFO
#define LOG_INFO(fmt, ...) Logger::getInstance().log(LogLevel::INFO, LOG_SOURCE_LOC, std::format(fmt, ##__VA_ARGS__))

/// Логирование уровня ERROR
#define LOG_ERROR(fmt, ...) Logger::getInstance().log(LogLevel::ERROR, LOG_SOURCE_LOC, std::format(fmt, ##__VA_ARGS__))

/// Логирование уровня WARN
#define LOG_WARN(fmt, ...) Logger::getInstance().log(LogLevel::WARN, LOG_SOURCE_LOC, std::format(fmt, ##__VA_ARGS__))

/// Логирование уровня DEBUG
#define LOG_DEBUG(fmt, ...) Logger::getInstance().log(LogLevel::DEBUG, LOG_SOURCE_LOC, std::format(fmt, ##__VA_ARGS__))

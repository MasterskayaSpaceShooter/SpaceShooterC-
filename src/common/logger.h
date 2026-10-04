#pragma once
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <format>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

/**
 * @brief Уровень логирования.
 */
enum class LogLevel { DEBUG, INFO, WARN, ERROR };

/**
 * @brief Метаданные о месте вызова лога.
 */
struct SourceLocation {
    const char* file{""};      ///< Имя файла (__FILE__)
    int line{0};               ///< Номер строки (__LINE__)
    const char* function{""};  ///< Имя функции (__FUNCTION__)
};

/**
 * @brief Потокобезопасный асинхронный логгер на boost::asio::strand.
 * @details Использует strand вместо std::mutex: strand не блокирует потоки,
 *          а сериализует задачи через очередь io_context.
 *          Logger — синглтон на статике, живёт до конца программы.
 */
class Logger {
public:
    /**
     * @brief Получить единственный экземпляр (Singleton).
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
     * @brief Инициализирует strand, привязывая его к io_context приложения.
     * @details Потокобезопасен и идемпотентен (std::once_flag + std::call_once).
     * @warning ДОЛЖЕН быть вызван до первого log() (для асинхронного пути).
     *          Если init() не вызван — log() идёт через fallback под fallback_mutex_.
     * @warning io_context должен жить дольше Logger, иначе strand_ становится dangling.
     */
    void init(boost::asio::io_context& io_context) {
        std::call_once(init_flag_, [this, &io_context]() {
            strand_ = std::make_unique<boost::asio::strand<boost::asio::io_context::executor_type>>(
                boost::asio::make_strand(io_context));
            initialized_.store(true, std::memory_order_release);
        });
    }

    /**
     * @brief Потокобезопасно ставит задачу печати лога в очередь strand_.
     * @details Если initialized_ == true — post в strand_ (асинхронно).
     *          Иначе — синхронный fallback под fallback_mutex_.
     * @note Logger — синглтон на статике, this валиден до конца программы.
     *       weak_from_this() не применяется — Logger не управляется shared_ptr.
     */
    void log(LogLevel level, const SourceLocation& loc, const std::string& message) {
        auto thread_id = std::this_thread::get_id();
        auto now = std::chrono::system_clock::now();
        if (initialized_.load(std::memory_order_acquire)) {
            boost::asio::post(*strand_, [this, now, thread_id, level, loc, message]() {
                try {
                    printToConsole(now, thread_id, level, loc, message);
                } catch (...) {
                    std::cerr << "Logger: exception in async handler\n";
                }
            });
        } else {
            std::lock_guard<std::mutex> lock(fallback_mutex_);
            printToConsole(now, thread_id, level, loc, message);
        }
    }

private:
    Logger() = default;

    /**
     * @brief Форматирует и выводит лог в консоль.
     * @details std::cout для не-ERROR, std::cerr для ERROR.
     */
    void printToConsole(std::chrono::system_clock::time_point timestamp,
                        std::thread::id thread_id,
                        LogLevel level,
                        const SourceLocation& loc,
                        const std::string& message) {
        // Время в формате YYYY-MM-DD HH:MM:SS.mmm
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

        // Уровень: выравниваем до 5 символов (INFO_, WARN_, ERROR, DEBUG).
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

        std::stringstream ss;
        ss << thread_id;
        const std::string thread_id_str = ss.str();

        // filename и module через std::string_view — без аллокаций.
        // "src/network/Session.cpp" → filename="Session.cpp", module="Session"
        std::string_view file_view{loc.file};
        auto slash = file_view.find_last_of("/\\");
        std::string_view filename = (slash == std::string_view::npos) ? file_view : file_view.substr(slash + 1);
        auto dot = filename.find_last_of('.');
        std::string_view module = (dot == std::string_view::npos) ? filename : filename.substr(0, dot);

        // Формат: [TIME][TH:xxxx][MODULE][FILE:LINE][FUNC][LEVEL] message
        std::string formatted = std::format("[{}][TH:{}][{}][{}:{}][{}()][{}] {}\n",
                                            time_str,
                                            thread_id_str,
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

    /// Strand для сериализации задач логирования. Заменяет std::mutex.
    std::unique_ptr<boost::asio::strand<boost::asio::io_context::executor_type>> strand_;

    /// Флаг готовности strand_. memory_order_release в init(), acquire в log().
    std::atomic<bool> initialized_{false};

    /// Мьютекс только для синхронного fallback (когда strand_ ещё не готов).
    std::mutex fallback_mutex_;

    /// Защита от повторной инициализации strand_.
    std::once_flag init_flag_;
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

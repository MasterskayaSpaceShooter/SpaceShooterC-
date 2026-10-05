#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <filesystem>
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
 *          strand_mutex_ защищает жизненный цикл strand_ (init/shutdown) —
 *          не сериализует вывод, а гарантирует, что strand_ не будет удалён
 *          во время использования.
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
     *          Взаимодействует с полями: strand_, initialized_, init_flag_.
     * @warning ДОЛЖЕН быть вызван до первого log() (для асинхронного пути).
     *          Если init() не вызван — log() работает через синхронный fallback
     *          под fallback_mutex_. Обычно вызывается в main() до io_context.run().
     * @warning io_context должен жить дольше Logger. Если io_context создан
     *          локально в main — перед выходом из его области видимости вызови
     *          Logger::getInstance().shutdown(). Иначе strand_ станет dangling
     *          (например, log() из статических деструкторов после main).
     * @warning Повторный init() после shutdown() НЕ восстанавливает strand_
     *          (std::call_once уже был выполнен). Если нужно переинициализировать —
     *          это требует сброса init_flag_, что не предусмотрено текущим API.
     * @param io_context Входные данные: Ссылка на io_context из NetworkContext.
     * @outputs Выходных значений нет.
     */
    void init(boost::asio::io_context& io_context) {
        std::call_once(init_flag_, [this, &io_context]() {
            std::lock_guard<std::mutex> lock(strand_mutex_);
            strand_ = std::make_unique<boost::asio::strand<boost::asio::io_context::executor_type>>(
                boost::asio::make_strand(io_context));
            initialized_.store(true, std::memory_order_release);
        });
    }

    /**
     * @brief Сбрасывает strand. Вызывать ДО разрушения io_context.
     * @details После shutdown() log() уходит в синхронный fallback под
     *          fallback_mutex_ — безопасно, даже если io_context уже мёртв.
     * @outputs Выходных значений нет.
     */
    void shutdown() {
        std::lock_guard<std::mutex> lock(strand_mutex_);
        initialized_.store(false, std::memory_order_release);
        strand_.reset();
    }

    /**
     * @brief Потокобезопасно ставит задачу печати лога в очередь strand_.
     * @details Если initialized_ == true — post в strand_ (асинхронно).
     *          Иначе — синхронный fallback под fallback_mutex_.
     *          Взаимодействует с полями: strand_, initialized_, fallback_mutex_.
     * @note Logger — синглтон на статике, живёт до конца программы. this валиден
     *       на момент выполнения handler'ов. weak_from_this() не применяется —
     *       Logger не управляется shared_ptr.
     * @note initialized_ и strand_ защищены strand_mutex_ при init()/shutdown()/log().
     *       memory_order_release/acquire — дополнительная синхронизация (согласование
     *       записи strand_ в init() с чтением в log()).
     * @note loc.file и loc.function копируются в std::string перед post,
     *       поэтому могут указывать на локальные данные.
     * @param level Входные данные: Уровень лога (DEBUG, INFO, WARN, ERROR).
     * @param loc Входные данные: Метаданные исходного кода (файл, строка, функция).
     * @param message Входные данные: Отформатированный текст сообщения.
     * @outputs Выходных значений нет.
     */
    void log(LogLevel level, const SourceLocation& loc, const std::string& message) {
        auto thread_id = std::this_thread::get_id();
        auto now = std::chrono::system_clock::now();

        std::lock_guard<std::mutex> lock(strand_mutex_);
        if (initialized_.load(std::memory_order_acquire)) {
            // MVP: копируем const char* в std::string, чтобы избежать dangling,
            // если вызывающий передал указатели на локальные данные.
            std::string file_str(loc.file);
            std::string func_str(loc.function);

            boost::asio::post(*strand_, [this, now, thread_id, level, file_str, func_str, loc, message]() {
                try {
                    printToConsole(now,
                                   thread_id,
                                   level,
                                   SourceLocation{file_str.c_str(), loc.line, func_str.c_str()},
                                   message);
                } catch (...) {
                    // аварийный вывод, чтобы не уронить io_context
                    std::cerr << "Logger: exception in async handler\n";
                }
            });
        } else {
            // Резервный синхронный вывод (если вызвали лог до инициализации Asio)
            std::lock_guard<std::mutex> lock(fallback_mutex_);
            printToConsole(now, thread_id, level, loc, message);
        }
    }

private:
    Logger() = default;

    /**
     * @brief Форматирует и выводит лог в консоль.
     * @details std::cout для не-ERROR, std::cerr для ERROR.
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

    /// Мьютекс для защиты доступа к strand_
    std::mutex strand_mutex_;

    /// Защита от повторной инициализации strand_.
    std::once_flag init_flag_;
};

// ============================================================================
// МАКРОСЫ С АВТОМАТИЧЕСКИМ ЗАХВАТОМ __FILE__, __LINE__, __FUNCTION__
// Используем __VA_OPT__ (C++20), а не GNU ##__VA_ARGS__.
//
// @warning Не передавайте в fmt-строку символы `{` или `}` как литералы,
//          если они не являются плейсхолдерами std::format. std::format
//          воспринимает `{}` как замену и падает с std::format_error
//          (или ошибкой компиляции), если аргумент не передан.
//          Для литеральных фигурных скобок используйте удвоение: {{ и }}.
//          Пример: LOG_INFO("Payload: {{action: 'connect'}}");  // OK
//                   LOG_INFO("Payload: {action: 'connect'}");    // ошибка
// ============================================================================

/// Вспомогательный макрос сборки SourceLocation
#define LOG_SOURCE_LOC                   \
    SourceLocation {                     \
        __FILE__, __LINE__, __FUNCTION__ \
    }

/// Логирование уровня INFO
#define LOG_INFO(fmt, ...) \
    Logger::getInstance().log(LogLevel::INFO, LOG_SOURCE_LOC, std::format(fmt __VA_OPT__(, ) __VA_ARGS__))

/// Логирование уровня ERROR
#define LOG_ERROR(fmt, ...) \
    Logger::getInstance().log(LogLevel::ERROR, LOG_SOURCE_LOC, std::format(fmt __VA_OPT__(, ) __VA_ARGS__))

/// Логирование уровня WARN
#define LOG_WARN(fmt, ...) \
    Logger::getInstance().log(LogLevel::WARN, LOG_SOURCE_LOC, std::format(fmt __VA_OPT__(, ) __VA_ARGS__))

/// Логирование уровня DEBUG
#define LOG_DEBUG(fmt, ...) \
    Logger::getInstance().log(LogLevel::DEBUG, LOG_SOURCE_LOC, std::format(fmt __VA_OPT__(, ) __VA_ARGS__))

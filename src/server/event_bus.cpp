#include "event_bus.h"

// Заглушки для логирования
#ifndef LOG_INFO
#define LOG_INFO(msg) (void)0
#endif

#ifndef LOG_ERROR
#define LOG_ERROR(msg) (void)0
#endif

namespace events {

// Заглушка базового события
struct Event {
    virtual ~Event() = default;
};
}
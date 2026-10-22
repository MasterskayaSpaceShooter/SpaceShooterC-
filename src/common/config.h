#pragma once

#include <cstdint>

#include "math_utils.h"

namespace world {
// Настройки игрового мира по умолчанию.

inline constexpr double kDefaultTickRateHz = 60.0;  ///< Частота тиков по умолчанию (60 FPS)
inline constexpr std::int64_t kMaxMissedTicks = 1;  ///< Допустимое отставание каденции в интервалах тика
inline constexpr math::Scalar kDefaultWidth = 2000.0;   ///< Ширина мира по умолчанию
inline constexpr math::Scalar kDefaultHeight = 2000.0;  ///< Высота мира по умолчанию
inline constexpr math::Scalar kDefaultClearance = 30.0;  ///< Зазор «запретной» окружности кандидата
}  // namespace world

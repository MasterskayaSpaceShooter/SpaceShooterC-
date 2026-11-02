#pragma once

#include <cstddef>
#include <cstdint>

#include "math_utils.h"

namespace world {
// Настройки игрового мира по умолчанию.

inline constexpr double kDefaultTickRateHz = 60.0;  ///< Частота тиков по умолчанию (60 FPS)
inline constexpr std::int64_t kMaxMissedTicks = 1;  ///< Допустимое отставание каденции в интервалах тика
inline constexpr math::Scalar kDefaultWidth = 2000.0;   ///< Ширина мира по умолчанию
inline constexpr math::Scalar kDefaultHeight = 2000.0;  ///< Высота мира по умолчанию
inline constexpr math::Scalar kDefaultClearance = 30.0;  ///< Зазор «запретной» окружности кандидата
inline constexpr std::size_t kMaxLeaderboardSize = 10;  /// Максимальное количество записей в таблице лидеров.

// Параметры корабля игрока по умолчанию.
inline constexpr math::Scalar kShipDefaultMass = 78.54;       ///< Масса корабля
inline constexpr math::Scalar kShipDefaultRadius = 5;         ///< Радиус корабля
inline constexpr math::Scalar kShipDefaultThrustPower = 7.5;  ///< Сила тяги корабля
inline constexpr math::Scalar kShipDefaultDrag = 0.125;  ///< Сопротивление движению корабля
inline constexpr std::uint16_t kShipDefaultHealth = 100;  ///< Максимальное здоровье корабля
inline constexpr std::uint16_t kShipDefaultDamage = 10;   ///< Урон корабля при столкновении
}  // namespace world

#pragma once

#include <cstdint>  // IWYU pragma: keep // std::uint

namespace world {

// Настройки игрового мира по умолчанию
// начало координат (0,0) - центр прямоугольника
inline constexpr double kDefaultWidth = 2000.0;
inline constexpr double kDefaultHeight = 2000.0;

/// Частота тиков по умолчанию (60 FPS)
inline constexpr double kDefaultTickRateHz = 60.0;
/// Допустимое отставание каденции в интервалах тика
inline constexpr std::int64_t kMaxMissedTicks = 1;
/// Зазор «запретной» окружности кандидата
inline constexpr double kDefaultClearance = 30.0;

// Параметры корабля игрока по умолчанию.
inline constexpr double kShipDefaultMass = 78.54;         ///< Масса корабля
inline constexpr double kShipDefaultRadius = 5;           ///< Радиус корабля
inline constexpr double kShipDefaultThrustPower = 7.5;    ///< Сила тяги корабля
inline constexpr double kShipDefaultDrag = 0.125;         ///< Сопротивление движению корабля
inline constexpr std::uint16_t kShipDefaultHealth = 100;  ///< Максимальное здоровье корабля
inline constexpr std::uint16_t kShipDefaultDamage = 10;   ///< Урон корабля при столкновении

}  // namespace world

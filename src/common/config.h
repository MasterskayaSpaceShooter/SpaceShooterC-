#pragma once

#include <cstddef>
#include <cstdint>

namespace world {

// Настройки игрового мира по умолчанию
// начало координат (0,0) - центр прямоугольника
inline constexpr double kDefaultWidth = 2000.0;
inline constexpr double kDefaultHeight = 2000.0;

inline constexpr double kDefaultTickRateHz = 60.0;  ///< Частота тиков по умолчанию (60 FPS)
inline constexpr std::int64_t kMaxMissedTicks = 1;  ///< Допустимое отставание каденции в интервалах тика
inline constexpr std::size_t kMaxLeaderboardSize = 10;  /// Максимальное количество записей в таблице лидеров.
inline constexpr double kDefaultClearance = 30.0;  ///< Зазор «запретной» окружности кандидата

// Параметры корабля игрока по умолчанию.
inline constexpr double kShipDefaultMass = 78.54;
inline constexpr double kShipDefaultRadius = 5;
/// Сила тяги корабля
inline constexpr double kShipDefaultThrustPower = 7.5;
/// Сопротивление движению корабля
inline constexpr double kShipDefaultDrag = 0.125;
inline constexpr std::uint16_t kShipDefaultHealth = 100;
inline constexpr std::uint16_t kShipDefaultDamage = 10;

// Параметры снаряда по умолчанию.
inline constexpr double kBulletSpeed = 500.0;  ///< Скорость снаряда
inline constexpr double kBulletRadius = 2.0;   ///< Радиус Снаряда (пули)
inline constexpr std::int64_t kBulletDamage = 25;  ///< Снаряды имеют стаблильное кол-во урона = 25 у.е.

}  // namespace world

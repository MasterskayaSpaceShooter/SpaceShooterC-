#pragma once

#include "entity.h"
#include "math_utils.h"

namespace domain::ship_physics {
/**
 * @brief Один шаг линейной физики корабля.
 *
 * @details Реализует модель движения twin-stick: направление тяги задаётся вектором
 *          пользовательского ввода, а угол корпуса автоматически разворачивается
 *          вслед за вектором скорости через atan2. Масса участвует в расчёте
 *          ускорения: a = F/m.
 * @warning Линейный drag FPS-зависим. На сервере dt должен быть фиксированным
 *          (1.0 / kDefaultTickRateHz). Не накапливать dt за пропущенные тики -
 *          вызывать stepShip несколько раз с kFixedDt.
 */
void stepShip(TransformComponent& transform, math::Vector2D move_direction, math::Scalar dt);

}  // namespace domain::ship_physics

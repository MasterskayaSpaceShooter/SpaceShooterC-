#pragma once

#include "math_utils.h"
namespace client {

/**
 * @brief Состояние ввода пользователя за текущий кадр.
 */
struct InputState {
    /// Направление движения корабля (нормализованный вектор или нулевой).
    math::Vector2D move_direction{0.0, 0.0};

    /// Целевой курсовой угол корабля (в радианах).
    math::Scalar target_angle{0.0};

    /// Флаг удержания кнопки стрельбы.
    bool is_shooting{false};

    /// Флаг нажатия кнопки возрождения (после экрана смерти).
    bool request_respawn{false};
};

/**
 * @brief Интерфейс поставщика пользовательского ввода.
 * @details Абстрагирует получение ввода от физических устройств (клавиатура, мышь, геймпад).
 */
class IInputProvider {
public:
    virtual ~IInputProvider() = default;

    /**
     * @brief Опрашивает текущее состояние устройств ввода.
     * @param my_ship_pos Текущая мировая позиция корабля игрока (для расчёта угла на курсор мыши).
     * @return Заполненная структура InputState.
     */
    virtual InputState pollInput(const math::Vector2D& my_ship_pos) = 0;
};

}  // namespace client

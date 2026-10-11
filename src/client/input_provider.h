#pragma once

namespace client {

/**
 * @brief Интерфейс источника пользовательского ввода.
 * @details Позволяет подменять реализацию: реальный ввод (SDL/SFML/GLFW)
 *          или заглушка (NullInputProvider) в тестах.
 */
class IInputProvider {
public:
    virtual ~IInputProvider() = default;

    /// Опрос текущего состояния ввода (клавиатура, мышь).
    virtual void pollInput() = 0;
};
}  // namespace client

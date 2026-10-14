#pragma once

#include "input_provider.h"

namespace client {

/**
 * @brief Заглушка IInputProvider — ничего не делает.
 * @details Используется в тестах и на ранних этапах, пока нет реального ввода.
 */
class NullInputProvider final : public IInputProvider {
public:
    void pollInput() override {}
};

}  // namespace client

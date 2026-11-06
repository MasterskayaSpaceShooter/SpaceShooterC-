#pragma once

#include <SFML/Graphics.hpp>
#include <cmath>
#include <numbers>

#include "input_provider.h"
#include "sfml_renderer.h"

namespace client {

class SfmlInputProvider : public IInputProvider {
public:
    SfmlInputProvider(const sf::RenderWindow& window, const SfmlRenderer& renderer)
        : window_(window), renderer_(renderer) {}

    InputState pollInput(const math::Vector2D& my_position) override {
        InputState state;

        // Положение мыши в игровых координатах мира
        const sf::Vector2i mouse_pixel = sf::Mouse::getPosition(window_);
        const sf::Vector2f mouse_world = renderer_.screenToWorld(mouse_pixel.x, mouse_pixel.y);

        const double dx = mouse_world.x - my_position.x;
        const double dy = mouse_world.y - my_position.y;
        state.target_angle = std::atan2(dy, dx);

        // Тяга (движение к курсору / W / Up / ПКМ)
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Up) ||
            sf::Mouse::isButtonPressed(sf::Mouse::Right)) {
            state.move_direction = math::Vector2D(std::cos(state.target_angle), std::sin(state.target_angle));
        }

        // Стрельба (Space / ЛКМ)
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Space) ||
            sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
            state.is_shooting = true;
        }

        // Респавн (клавиша R)
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::R)) {
            state.request_respawn = true;
        }

        return state;
    }

private:
    const sf::RenderWindow& window_;
    const SfmlRenderer& renderer_;
};

}  // namespace client

#pragma once

#include <SFML/Graphics.hpp>
#include <cmath>
#include <numbers>
#include <string>

#include "client_world.h"
#include "renderer.h"

namespace client {

class SfmlRenderer : public IRenderer {
public:
    explicit SfmlRenderer(sf::RenderWindow& window) : window_(window) {
        has_font_ = font_.loadFromFile("resources/sansation.ttf");
        if (!has_font_) {
            has_font_ = font_.loadFromFile("C:\\Windows\\Fonts\\arial.ttf");
        }
    }

    void drawCircle(double x, double y, double radius, uint32_t color_rgba) {
        sf::CircleShape circle(static_cast<float>(radius));
        circle.setOrigin(static_cast<float>(radius), static_cast<float>(radius));
        circle.setPosition(worldToScreen(x, y));
        circle.setFillColor(sf::Color(color_rgba));
        window_.draw(circle);
    }

    void drawShip(double x, double y, double radius, double angle_rad, uint32_t color_rgba) {
        sf::ConvexShape ship;
        ship.setPointCount(3);
        const float r = static_cast<float>(radius);
        ship.setPoint(0, sf::Vector2f(r * 1.6f, 0.0f));
        ship.setPoint(1, sf::Vector2f(-r, -r * 0.8f));
        ship.setPoint(2, sf::Vector2f(-r, r * 0.8f));

        ship.setOrigin(0.0f, 0.0f);
        ship.setPosition(worldToScreen(x, y));
        // В SFML угол по часовой стрелке в градусах
        ship.setRotation(static_cast<float>(angle_rad * 180.0 / std::numbers::pi));
        ship.setFillColor(sf::Color(color_rgba));
        ship.setOutlineColor(sf::Color::White);
        ship.setOutlineThickness(2.0f);
        window_.draw(ship);
    }

    void drawText(const std::string& text, int screen_x, int screen_y) override {
        if (!has_font_) return;
        sf::Text sf_text;
        sf_text.setFont(font_);
        sf_text.setString(text);
        sf_text.setCharacterSize(18);
        sf_text.setFillColor(sf::Color::White);
        sf_text.setPosition(static_cast<float>(screen_x), static_cast<float>(screen_y));
        window_.draw(sf_text);
    }

    void renderWelcomeWindow(const std::string& player_name) override {
        window_.clear(sf::Color(12, 14, 28));
        drawText("Connecting as: " + player_name + "...", 50, 50);
        window_.display();
    }

    void renderGame(const ClientWorld& world) override {
        window_.clear(sf::Color(10, 12, 24));

        // 1. Центрируем камеру на нашем корабле (если он есть в мире)
        math::Vector2D focus(0.0, 0.0);
        for (const auto& [id, entity] : world.getEntities()) {
            if (entity.type == types::EntityType::Ship && entity.transform.has_value() && !entity.is_destroyed) {
                focus = entity.transform->position;
                break;
            }
        }
        camera_offset_ = focus;

        // 2. Отрисовка координатной сетки для наглядного ощущения скорости/движения
        drawBackgroundGrid();

        // 3. Отрисовка всех сущностей
        for (const auto& [id, entity] : world.getEntities()) {
            if (entity.is_destroyed || !entity.transform.has_value()) {
                continue;
            }

            const auto& pos = entity.transform->position;
            const auto angle = entity.transform->angle;

            if (entity.type == types::EntityType::Ship) {
                drawShip(pos.x, pos.y, entity.radius > 0.0 ? entity.radius : 20.0, angle, 0x00FF66FF);
            } else if (entity.type == types::EntityType::Bullet) {
                // Пули делаем крупнее (радиус 5px) и ярко-жёлтыми с белой обводкой
                sf::CircleShape bullet_shape(5.0f);
                bullet_shape.setOrigin(5.0f, 5.0f);
                bullet_shape.setPosition(worldToScreen(pos.x, pos.y));
                bullet_shape.setFillColor(sf::Color::Yellow);
                bullet_shape.setOutlineColor(sf::Color::White);
                bullet_shape.setOutlineThickness(1.5f);
                window_.draw(bullet_shape);
            } else if (entity.type == types::EntityType::Asteroid) {
                // Астероиды серого цвета с рельефной обводкой
                sf::CircleShape ast_shape(static_cast<float>(entity.radius > 0.0 ? entity.radius : 30.0));
                ast_shape.setOrigin(ast_shape.getRadius(), ast_shape.getRadius());
                ast_shape.setPosition(worldToScreen(pos.x, pos.y));
                ast_shape.setFillColor(sf::Color(100, 105, 115));
                ast_shape.setOutlineColor(sf::Color(170, 175, 185));
                ast_shape.setOutlineThickness(2.0f);
                window_.draw(ast_shape);
            }
        }

        window_.display();
    }

    void renderDeathScreen(std::uint32_t final_score) override {
        window_.clear(sf::Color(45, 12, 12));
        drawText("YOU WERE DESTROYED!", 380, 300);
        drawText("Score: " + std::to_string(final_score), 380, 340);
        drawText("Press 'R' to Respawn", 380, 380);
        window_.display();
    }

    [[nodiscard]] sf::Vector2f worldToScreen(double wx, double wy) const {
        const auto size = window_.getSize();
        // Преобразуем мировые координаты относительно смещения камеры
        return sf::Vector2f(
            static_cast<float>((wx - camera_offset_.x) + size.x * 0.5f),
            static_cast<float>(-(wy - camera_offset_.y) + size.y * 0.5f)
        );
    }

    [[nodiscard]] sf::Vector2f screenToWorld(int sx, int sy) const {
        const auto size = window_.getSize();
        return sf::Vector2f(
            static_cast<float>((sx - static_cast<int>(size.x / 2)) + camera_offset_.x),
            static_cast<float>(-(sy - static_cast<int>(size.y / 2)) + camera_offset_.y)
        );
    }

private:
    void drawBackgroundGrid() {
        const auto size = window_.getSize();
        constexpr float step = 100.0f;

        float start_x = std::fmod(-static_cast<float>(camera_offset_.x) + size.x * 0.5f, step);
        float start_y = std::fmod(static_cast<float>(camera_offset_.y) + size.y * 0.5f, step);

        sf::VertexArray lines(sf::Lines);
        for (float x = start_x; x < size.x; x += step) {
            lines.append(sf::Vertex(sf::Vector2f(x, 0.0f), sf::Color(30, 35, 55)));
            lines.append(sf::Vertex(sf::Vector2f(x, static_cast<float>(size.y)), sf::Color(30, 35, 55)));
        }
        for (float y = start_y; y < size.y; y += step) {
            lines.append(sf::Vertex(sf::Vector2f(0.0f, y), sf::Color(30, 35, 55)));
            lines.append(sf::Vertex(sf::Vector2f(static_cast<float>(size.x), y), sf::Color(30, 35, 55)));
        }
        window_.draw(lines);
    }

private:
    sf::RenderWindow& window_;
    sf::Font font_;
    bool has_font_{false};
    math::Vector2D camera_offset_{0.0, 0.0};
};

}  // namespace client

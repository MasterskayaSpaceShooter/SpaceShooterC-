#pragma once

#include <algorithm>
#include <cstdint>

#include "math_utils.h"

class Spaceship {
public:
    Spaceship() = default;

    Spaceship(math::Vector2D position,
              math::Scalar current_mass,
              math::Scalar max_mass,
              std::uint16_t current_hp,
              std::uint16_t max_hp,
              math::Scalar radius,
              std::uint16_t score,
              std::uint16_t xp) :
        position_(position), spawn_position_(position), current_mass_(current_mass), max_mass_(max_mass),
        current_hp_(current_hp), max_hp_(max_hp), radius_(radius), score_(score), xp_(xp) {}

    void update(math::Scalar dt) {
        position_ += velocity_ * dt;
    }

    void takeDamage(std::uint16_t amount) {
        current_hp_ = amount >= current_hp_ ? 0 : static_cast<std::uint16_t>(current_hp_ - amount);
    }

    void addMass(math::Scalar amount) {
        current_mass_ = std::clamp(current_mass_ + amount, math::Scalar{0}, max_mass_);
    }

    void updatePosition(bool respawn) {
        if (respawn) {
            position_ = spawn_position_;
            velocity_ = {};
        }
    }

    void setPosition(const math::Vector2D& position) {
        position_ = position;
    }
    void setVelocity(const math::Vector2D& velocity) {
        velocity_ = velocity;
    }
    void setCurrentMass(math::Scalar mass) {
        current_mass_ = mass;
    }
    void setMaxMass(math::Scalar mass) {
        max_mass_ = mass;
    }
    void setCurrentHp(std::uint16_t hp) {
        current_hp_ = hp;
    }
    void setMaxHp(std::uint16_t hp) {
        max_hp_ = hp;
    }
    void setRadius(math::Scalar radius) {
        radius_ = radius;
    }
    void setScore(std::uint16_t score) {
        score_ = score;
    }
    void setXp(std::uint16_t xp) {
        xp_ = xp;
    }

    [[nodiscard]] math::Vector2D getPosition() const {
        return position_;
    }
    [[nodiscard]] math::Vector2D getVelocity() const {
        return velocity_;
    }
    [[nodiscard]] math::Scalar getCurrentMass() const {
        return current_mass_;
    }
    [[nodiscard]] math::Scalar getMaxMass() const {
        return max_mass_;
    }
    [[nodiscard]] std::uint16_t getCurrentHp() const {
        return current_hp_;
    }
    [[nodiscard]] std::uint16_t getMaxHp() const {
        return max_hp_;
    }
    [[nodiscard]] math::Scalar getRadius() const {
        return radius_;
    }
    [[nodiscard]] std::uint16_t getScore() const {
        return score_;
    }
    [[nodiscard]] std::uint16_t getXp() const {
        return xp_;
    }

private:
    math::Vector2D position_{};
    math::Vector2D spawn_position_{};
    math::Vector2D velocity_{};
    math::Scalar current_mass_{0};
    math::Scalar max_mass_{0};
    std::uint16_t current_hp_{0};
    std::uint16_t max_hp_{0};
    math::Scalar radius_{0};
    std::uint16_t score_{0};
    std::uint16_t xp_{0};
};

#pragma once

#include <cstdint>
#include <limits>

#include "entity.h"

namespace domain {

inline void addScore(ShipComponent& ship, std::uint32_t points) noexcept {
    const auto max_score = std::numeric_limits<std::uint32_t>::max();

    ship.score = (points > max_score - ship.score) ? max_score : ship.score + points;
}

inline void subtractScore(ShipComponent& ship, std::uint32_t penalty) noexcept {
    ship.score = (penalty >= ship.score) ? 0 : ship.score - penalty;
}

}  // namespace domain

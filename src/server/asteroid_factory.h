#pragma once
#include <unordered_map>
#include <memory>
#include <vector>
#include <entity_snapshot.h>
#include <random>
#include "math_utils.h"

namespace asteroid_factory{
constexpr size_t MIN_ASTEROIDS_WEIGHT = 500;
constexpr size_t MIN_ASTEROID_WEIGHT = 10;
constexpr size_t MAX_ASTEROID_WEIGHT = 80;

struct AsteroidData {
    size_t weight;
    size_t score_reward;
    math::Vector2D coordinate;
    
};

using AsteroidInfoPtr = std::shared_ptr<std::unordered_map<snapshot::EntityId, AsteroidData>>;


class AsteroidFactory {
public:
    using AsteroidData = asteroid_factory::AsteroidData;

    AsteroidFactory(AsteroidInfoPtr asteroids);
    void update(const std::vector<math::Vector2D>& free_coordinates);
    AsteroidData createRandomAsteroid(const math::Vector2D& coordinate) const;
    std::vector<AsteroidData> createSplitSteroid(const AsteroidData& parent);
    std::vector<size_t> generateAsteroidWeight();
    std::vector<math::Vector2D> generatePositions(const size_t pos_count, const std::vector<math::Vector2D>& free_coordinates);
private:
    AsteroidInfoPtr asteroids_;
    size_t asteroids_weight_;

};

}


#pragma once
#include <entity_snapshot.h>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

#include "entity_pool.h"
#include "math_utils.h"
#include "types.h"

namespace asteroid_factory {
constexpr double MIN_ASTEROIDS_WEIGHT = 500.0;
constexpr double MIN_ASTEROID_WEIGHT = 10.0;
constexpr double MAX_ASTEROID_WEIGHT = 80.0;

struct AsteroidData {
    domain::Entity* entity;
    math::Scalar mass;
};

using AsteroidInfoPtr = std::shared_ptr<std::unordered_map<types::EntityId, AsteroidData>>;

class AsteroidFactory {
public:
    using AsteroidData = asteroid_factory::AsteroidData;

    AsteroidFactory(AsteroidInfoPtr asteroids);
    void update(const std::vector<math::Vector2D>& free_coordinates);
    void onDestroyed(const std::vector<types::EntityId>& asteroidsToBeDestroyed);
    // AsteroidData createRandomAsteroid(const math::Vector2D& coordinate) const;
    std::vector<AsteroidData> createSplitSteroid(const AsteroidData& parent);
    std::vector<double> generateAsteroidWeight();
    std::vector<math::Vector2D> generatePositions(const size_t pos_count,
                                                  const std::vector<math::Vector2D>& free_coordinates);

private:
    AsteroidInfoPtr asteroids_;
    double asteroids_weight_;
    domain::EntityPool entity_pool_;
};

}  // namespace asteroid_factory

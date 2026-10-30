#pragma once
#include <entity_snapshot.h>
#include <memory>
#include <random>
#include <vector>

#include "entity_pool.h"
#include "math_utils.h"
#include "types.h"

namespace asteroid_factory {
constexpr double MIN_ASTEROIDS_WEIGHT = 500.0;
constexpr double MIN_ASTEROID_WEIGHT = 10.0;
constexpr double MAX_ASTEROID_WEIGHT = 80.0;
constexpr double MAX_WEIGHT_FOR_RADIUS = 30.0;
constexpr double MAX_RADIUS_COUNTER = 30.0;
constexpr double MIN_RADIUS_COUNTER = 30.0;

struct AsteroidData {
    domain::Entity* entity;
    math::Scalar mass;
};

class AsteroidFactory {
public:
    AsteroidFactory(std::shared_ptr<domain::EntityPool> entity_pool);
    void update(const std::vector<math::Vector2D>& free_coordinates);
    std::vector<AsteroidData> createSplitAsteroid(const AsteroidData& parent);
    std::vector<double> generateAsteroidWeight(const double current_weight);
    std::vector<math::Vector2D> generatePositions(const size_t pos_count,
                                                  const std::vector<math::Vector2D>& free_coordinates);

private:
    double calculateCurrentWeight() const;

private:
    std::shared_ptr<domain::EntityPool> entity_pool_;
};

}  // namespace asteroid_factory

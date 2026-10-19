#include "asteroid_factory.h"

namespace asteroid_factory {

thread_local std::mt19937 g_rng{std::random_device {}()};

AsteroidFactory::AsteroidFactory(AsteroidInfoPtr asteroids) : asteroids_(asteroids), asteroids_weight_(0) {
    generateAsteroidWeight();
}

void AsteroidFactory::update(const std::vector<math::Vector2D>& free_coordinates) {
    if (asteroids_weight_ >= MIN_ASTEROIDS_WEIGHT) {
        return;
    }
    //generate_weight
    std::vector<size_t> free_weight = generateAsteroidWeight();

    //generate_positions
    std::vector<math::Vector2D> rand_positions = generatePositions(free_weight.size(), free_coordinates);

    //create_random_asteroids
    for (size_t i = 0; i < free_weight.size(); ++i) {
        
    }
}

AsteroidData AsteroidFactory::createRandomAsteroid(const math::Vector2D& coordinate) const {
    return AsteroidFactory::AsteroidData();
}

std::vector<AsteroidData> AsteroidFactory::createSplitSteroid(const AsteroidData& parent) {
    return std::vector<AsteroidFactory::AsteroidData>();
}

std::vector<size_t> AsteroidFactory::generateAsteroidWeight() {
    size_t min = MIN_ASTEROIDS_WEIGHT - asteroids_weight_;
    std::uniform_int_distribution<size_t> distrib(min, MIN_ASTEROIDS_WEIGHT);
    size_t rand_weight = distrib(g_rng);
    std::vector<size_t> asteroids_weight;
    while (rand_weight >= MIN_ASTEROID_WEIGHT) {
        std::uniform_int_distribution<size_t> distrib(MIN_ASTEROID_WEIGHT, MAX_ASTEROID_WEIGHT);
        size_t solo_weight = distrib(g_rng);
        asteroids_weight.push_back(solo_weight);
        rand_weight -= solo_weight;
    }
    return asteroids_weight;
}

std::vector<math::Vector2D> AsteroidFactory::generatePositions(const size_t pos_count, const std::vector<math::Vector2D>& free_coordinates) {
    std::vector<math::Vector2D> positions;
    positions.reserve(pos_count);
    std::uniform_int_distribution<size_t> distrib(0, free_coordinates.size() - 1);
    for(size_t i = 0; i < positions.size(); ++i){
        size_t rand_coordinate = distrib(g_rng);
        positions.push_back(free_coordinates[rand_coordinate]);
    }
    return positions;
}
}  // namespace asteroid_factory

#include "asteroid_factory.h"

#include <algorithm>

namespace asteroid_factory {

thread_local std::mt19937 g_rng{std::random_device{}()};

AsteroidFactory::AsteroidFactory(AsteroidInfoPtr asteroids) : asteroids_(asteroids), asteroids_weight_(0) {}

void AsteroidFactory::update(const std::vector<math::Vector2D>& free_coordinates) {
    entity_pool_.recycleDestroyed();
    if (asteroids_weight_ >= MIN_ASTEROIDS_WEIGHT) {
        return;
    }
    // generate_weight
    std::vector<double> free_weight = generateAsteroidWeight();
    if (free_coordinates.empty()) return;

    // generate_positions
    std::vector<math::Vector2D> rand_positions = generatePositions(free_weight.size(), free_coordinates);
    if (free_weight.empty()) return;
    
    // create_random_asteroids
    std::vector<AsteroidData> asteroids;
    asteroids.reserve(rand_positions.size());
    for (size_t i = 0; i < free_weight.size(); ++i) {
        domain::Entity* entity = entity_pool_.acquire();
        entity->type = domain::EntityType::Asteroid;
        entity->mass = free_weight[i];
        entity->radius = free_weight[i] < 30.0 ? free_weight[i] * 5.0 : free_weight[i] * 2.5;
        entity->transform.emplace();
        entity->asteroid.emplace();
        entity->asteroid->score_reward = static_cast<uint16_t>(free_weight[i]);
        entity->transform->position = rand_positions[i];

        asteroids.push_back({.entity = entity, .mass = free_weight[i]});
        AsteroidData asteroid{entity};

        asteroids_->insert({entity->id, asteroid});
        asteroids_weight_ += entity->mass;
    }
}

//this method must call before the update method
void AsteroidFactory::onDestroyed(const std::vector<snapshot::EntityId>& asteroidsToBeDestroyed) {
    if (asteroidsToBeDestroyed.empty()) {
        return;
    }

    for (size_t i = 0; i < asteroidsToBeDestroyed.size(); i++) {
        auto it = asteroids_->find(asteroidsToBeDestroyed[i]);
        if (it == asteroids_->end()) {
            continue;
        }
        asteroids_weight_ -= it->second.mass;
        it->second.entity->isDestroyed = true;
        asteroids_->erase(asteroidsToBeDestroyed[i]);
    }
}

/*AsteroidData AsteroidFactory::createRandomAsteroid(const math::Vector2D& coordinate) const {
    return AsteroidFactory::AsteroidData();
}*/

std::vector<AsteroidData> AsteroidFactory::createSplitSteroid(const AsteroidData& parent) {
    std::vector<AsteroidFactory::AsteroidData> new_asteroids;
}

std::vector<double> AsteroidFactory::generateAsteroidWeight() {
    std::vector<double> asteroids_weight;
    if (asteroids_weight_ >= MIN_ASTEROIDS_WEIGHT) {
        return asteroids_weight;
    }
    const size_t budget_left = MIN_ASTEROIDS_WEIGHT - asteroids_weight_;
    if (budget_left < MIN_ASTEROID_WEIGHT) {
        return asteroids_weight;
    }
    std::uniform_int_distribution<size_t> distrib(MIN_ASTEROID_WEIGHT, budget_left);
    double remaining = distrib(g_rng);
    while (remaining >= MIN_ASTEROID_WEIGHT) {
        const double upper = std::min(MAX_ASTEROID_WEIGHT, remaining);
        std::uniform_int_distribution<size_t> solo_distrib(MIN_ASTEROID_WEIGHT, upper);
        const double solo_weight = solo_distrib(g_rng);
        asteroids_weight.push_back(solo_weight);
        remaining -= solo_weight;
    }
    return asteroids_weight;
}

std::vector<math::Vector2D> AsteroidFactory::generatePositions(const size_t pos_count,
                                                               const std::vector<math::Vector2D>& free_coordinates) {
    if (pos_count == 0 || free_coordinates.empty()) return {};
    std::vector<math::Vector2D> positions;
    positions.reserve(pos_count);
    std::uniform_int_distribution<size_t> distrib(0, free_coordinates.size() - 1);
    for (size_t i = 0; i < pos_count; ++i) {
        size_t rand_coordinate = distrib(g_rng);
        positions.push_back(free_coordinates[rand_coordinate]);
    }
    return positions;
}
}  // namespace asteroid_factory

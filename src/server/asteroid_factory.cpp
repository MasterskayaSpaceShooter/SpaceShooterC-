#include "asteroid_factory.h"

#include <algorithm>

namespace asteroid_factory {

thread_local std::mt19937 g_rng{std::random_device{}()};

AsteroidFactory::AsteroidFactory(std::shared_ptr<domain::EntityPool> entity_pool) : entity_pool_(entity_pool) {}

void AsteroidFactory::update(const std::vector<math::Vector2D>& free_coordinates) {
    if (free_coordinates.empty()) {
        return;
    }

    const double current_weight = calculateCurrentWeight();

    if (current_weight >= MIN_ASTEROIDS_WEIGHT) {
        return;
    }

    // generate_weight
    std::vector<double> free_weight = generateAsteroidWeight(current_weight);
    if (free_coordinates.empty())
        return;

    // generate_positions
    std::vector<math::Vector2D> rand_positions = generatePositions(free_weight.size(), free_coordinates);
    if (free_weight.empty())
        return;

    // create_random_asteroids
    for (size_t i = 0; i < free_weight.size(); ++i) {
        domain::Entity* entity = entity_pool_->acquire();
        entity->type = types::EntityType::Asteroid;
        entity->mass = free_weight[i];
        entity->radius = free_weight[i] < 30.0 ? free_weight[i] * 5.0 : free_weight[i] * 2.5;
        entity->transform.emplace();
        entity->asteroid.emplace();
        entity->health.emplace();
        entity->asteroid->score_reward = static_cast<uint16_t>(free_weight[i]);
        entity->transform->position = rand_positions[i];
    }
}

double AsteroidFactory::calculateCurrentWeight() const {
    double total_weight = 0.0;

    for (const auto& slot : entity_pool_->storage()) {
        if (slot.has_value() && !slot->isDestroyed && slot->type == types::EntityType::Asteroid) {
            total_weight += slot->mass;
        }
    }
    return total_weight;
}

std::vector<AsteroidData> AsteroidFactory::createSplitAsteroid([[maybe_unused]] const AsteroidData& parent) {
    return {};
}

std::vector<double> AsteroidFactory::generateAsteroidWeight(const double current_weight) {
    std::vector<double> asteroids_weight;
    if (current_weight >= MIN_ASTEROIDS_WEIGHT) {
        return asteroids_weight;
    }
    const double budget_left = MIN_ASTEROIDS_WEIGHT - current_weight;
    if (budget_left < MIN_ASTEROID_WEIGHT) {
        return asteroids_weight;
    }
    std::uniform_real_distribution<double> distrib(MIN_ASTEROID_WEIGHT, budget_left);
    double remaining = distrib(g_rng);
    while (remaining >= MIN_ASTEROID_WEIGHT) {
        const double upper = std::min(MAX_ASTEROID_WEIGHT, remaining);
        std::uniform_real_distribution<double> solo_distrib(MIN_ASTEROID_WEIGHT, upper);
        const double solo_weight = solo_distrib(g_rng);
        asteroids_weight.push_back(solo_weight);
        remaining -= solo_weight;
    }
    return asteroids_weight;
}

std::vector<math::Vector2D> AsteroidFactory::generatePositions(const size_t pos_count,
                                                               const std::vector<math::Vector2D>& free_coordinates) {
    if (pos_count == 0 || free_coordinates.empty() || pos_count > free_coordinates.size())
        return {};
    std::vector<math::Vector2D> positions;
    positions.reserve(pos_count);
    std::sample(free_coordinates.begin(), free_coordinates.end(), std::back_inserter(positions), pos_count, g_rng);
    return positions;
}
}  // namespace asteroid_factory

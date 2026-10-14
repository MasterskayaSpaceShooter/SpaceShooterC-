#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include "entity.h"

namespace domain {

/**
 * @brief Fixed-slot pool for Entity objects, backed by std::deque for stable addresses.
 *
 * @invariant storage_.size() == high-water mark of simultaneously live entities.
 * @invariant Indices in free_indices_ are unique and refer to slots whose
 *            isDestroyed flag has been cleared by recycleDestroyed().
 * @note Thread-unsafe. Call from the simulation thread only.
 *
 * @invariant A slot is "free" if it is in free_indices_, and while free
 *            it holds isDestroyed == true and type == None.
 *            acquire() resets the slot with `storage_[idx] = Entity{},
 *            which is the only place isDestroyed is cleared.
 */
class EntityPool {
public:
    /**
     * @brief Obtain a reset Entity slot, reusing a freed slot when possible.
     * @return Non-null pointer to a zero-initialized Entity with a fresh id.
     * @post Returned entity has isDestroyed == false and no optional components set.
     * @post type == EntityType::None until the caller assigns it.
     * @note The returned pointer stays valid across subsequent acquire() calls
     */
    Entity* acquire() {
        if (!free_indices_.empty()) {
            const auto idx = free_indices_.back();
            free_indices_.pop_back();
            storage_[idx] = Entity{};  // reset
            storage_[idx].id = next_id_++;
            return &storage_[idx];
        }
        storage_.emplace_back();
        storage_.back().id = next_id_++;
        return &storage_.back();
    }

    /**
     * @brief Reclaim every slot currently flagged isDestroyed.
     * @pre Called once per frame after all systems have finished writing flags.
     * @post All previously destroyed slots are marked live (isDestroyed == false)
     *       and queued for reuse by acquire(). Idempotent: calling twice without
     *       any destruction in between is a no-op.
     * @warning Any raw pointer/index to a recycled slot held by other systems
     *          becomes stale after this call.
     */
    void recycleDestroyed() {
        for (std::size_t i = 0; i < storage_.size(); ++i) {
            Entity& e = storage_[i];
            if (e.isDestroyed && e.type != EntityType::None) {
                free_indices_.push_back(i);
                e.type = EntityType::None;
            }
        }
    }

    /// Direct access to storage for iteration. Must skip isDestroyed entities.
    std::deque<Entity>& storage() {
        return storage_;
    }

private:
    std::deque<Entity> storage_;
    std::vector<std::size_t> free_indices_;
    uint64_t next_id_{1};
};

}  // namespace domain

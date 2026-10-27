#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

#include "entity.h"

namespace domain {

/**
 * @brief Fixed-slot pool for Entity objects, backed by std::deque for stable addresses.
 *
 * @invariant storage_.size() is the high-water mark of simultaneously engaged slots.
 * @invariant free_indices_ and disengaged slots form a bijection:
 *            idx is in free_indices_ iff storage_[idx] == std::nullopt.
 * @invariant A slot is engaged iff storage_[idx].has_value().
 *            An engaged slot may still have isDestroyed == true until
 *            recycleDestroyed() runs.
 * @note Entity must value-initialize to isDestroyed == false; acquire()
 *       relies on this via std::optional::emplace().
 * @note Thread-unsafe. Call from the simulation thread only. Const-методы
 *       (storage(), isActive(), activeCount()) тоже не потокобезопасны: они
 *       читают storage_/free_indices_ без блокировок, и их вызов небезопасен
 *       при конкурентной мутации пула из другого потока. В проекте всю работу
 *       с пулом сериализует tick_strand_ WorldInstance.
 */
class EntityPool {
public:
    /**
     * @brief Obtain a fresh Entity slot, reusing a freed slot when possible.
     * @return Non-null pointer to a value-initialized Entity with a fresh id.
     * @note The pointer remains valid until recycleDestroyed() recycles this
     *       slot (see class-level @note on Entity value-initialization).
     */
    Entity* acquire() {
        if (!free_indices_.empty()) {
            const auto idx = free_indices_.back();
            free_indices_.pop_back();
            storage_[idx].emplace();
            storage_[idx]->id = types::EntityId{next_id_++};
            return &*storage_[idx];
        }

        storage_.emplace_back();
        storage_.back().emplace();
        storage_.back()->id = types::EntityId{next_id_++};
        return &*storage_.back();
    }

    /**
     * @brief Reclaim every live slot currently flagged isDestroyed.
     * @pre Called once per frame after all systems have finished writing flags.
     * @post All previously destroyed slots are disengaged (std::nullopt) and
     *       queued for reuse by acquire().
     * @post Idempotent: calling twice without any destruction in between is a no-op.
     * @warning Recycling a slot destroys its contained Entity. Any Entity*
     *          previously returned by acquire() for that slot becomes dangling.
     *          Any index to a recycled slot now refers to a disengaged slot
     *          that a later acquire() may reuse for a different entity.
     */
    void recycleDestroyed() {
        for (std::size_t i = 0; i < storage_.size(); ++i) {
            auto& slot = storage_[i];
            if (slot && slot->isDestroyed) {
                free_indices_.push_back(i);
                slot.reset();
            }
        }
    }

    /// Direct access to storage for iteration.
    /// Callers must skip empty slots and slots whose entity is flagged isDestroyed.
    std::deque<std::optional<Entity>>& storage() {
        return storage_;
    }

    /// Const access to storage for iteration.
    /// Callers must skip empty slots and slots whose entity is flagged isDestroyed.
    /// @note Const не означает потокобезопасность: см. классовый @note.
    const std::deque<std::optional<Entity>>& storage() const {
        return storage_;
    }

    /**
     * @brief Активен ли слот пула: занят сущностью, не помеченной на удаление.
     * @details Сущность с типом EntityType::None считается неинициализированной
     *          и неактивной.
     * @param slot Входные данные: Слот хранилища пула.
     * @return true Если slot.has_value() && slot->type != EntityType::None && !slot->isDestroyed.
     * @note Не потокобезопасен: читает слот без блокировок, см. классовый @note.
     */
    [[nodiscard]] bool isActive(const std::optional<Entity>& slot) const noexcept {
        return slot.has_value() && slot->type != types::EntityType::None && !slot->isDestroyed;
    }

    /**
     * @brief Количество занятых слотов пула (верхняя граница числа активных сущностей).
     * @details O(1): storage_.size() — high-water mark пула, free_indices_.size() —
     *          число освобождённых слотов. После recycleDestroyed() значение совпадает
     *          с числом активных сущностей; между пометкой isDestroyed и рециклингом
     *          оно может быть чуть больше.
     * @note Не потокобезопасен: читает storage_/free_indices_ без блокировок,
     *       см. классовый @note.
     */
    [[nodiscard]] std::size_t activeCount() const noexcept {
        return storage_.size() - free_indices_.size();
    }

private:
    std::deque<std::optional<Entity>> storage_;
    std::vector<std::size_t> free_indices_;
    uint64_t next_id_{1};
};

}  // namespace domain

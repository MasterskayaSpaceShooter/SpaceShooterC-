#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "entity_snapshot.h"

namespace snapshot {
struct LeaderboardEntry {
    std::string name;
    std::uint32_t score;
};

class GameSnapshot {
public:
    GameSnapshot(std::vector<EntitySnapshot> snapshots,
                 std::vector<LeaderboardEntry> leaderboard,
                 std::uint32_t tick_number) :
        snapshots_(std::move(snapshots)), leaderboard_(std::move(leaderboard)), tick_number_(tick_number) {}
    GameSnapshot() = default;

    GameSnapshot(const GameSnapshot&) = delete;
    GameSnapshot& operator=(const GameSnapshot&) = delete;

    GameSnapshot(GameSnapshot&&) noexcept = default;
    GameSnapshot& operator=(GameSnapshot&&) noexcept = default;

    // Возвращает вектор снимков сущностей, которые были зафиксированы на момент тика tick_number_.
    [[nodiscard]] const std::vector<EntitySnapshot>& getSnapshots() const noexcept {
        return snapshots_;
    }

    [[nodiscard]] const std::vector<LeaderboardEntry>& getLeaderboard() const noexcept {
        return leaderboard_;
    }

    // Возвращает номер тика, на момент которого был зафиксирован снимок.
    [[nodiscard]] std::uint32_t getTickNumber() const noexcept {
        return tick_number_;
    }

    // Порядок передачи данных: тик и количество сущностей, затем фиксированные поля каждой сущности и
    // имя в поле длиной 32 байта. Числовые значения кодируются в сетевом порядке байтов.
    // При десериализации отклоняются некорректные данные и все завершающие байты.
    [[nodiscard]] std::vector<std::uint8_t> Serialize() const;
    [[nodiscard]] static GameSnapshot Deserialize(const std::vector<std::uint8_t>& buffer);

private:
    std::vector<EntitySnapshot> snapshots_;
    std::vector<LeaderboardEntry> leaderboard_;
    std::uint32_t tick_number_;
};
}  // namespace snapshot

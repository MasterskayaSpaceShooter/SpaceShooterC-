#include "entity_snapshot_serder.h"

#include <cassert>
#include <string>

#include "entity_snapshot_generated.h"

namespace snapshot_wire {

namespace {

constexpr std::size_t kFlatBufferIdOffset = sizeof(flatbuffers::uoffset_t);  // 4
constexpr std::size_t kFileIdentifierLength = 4;
constexpr std::size_t kMinFlatBufferSize = kFlatBufferIdOffset + kFileIdentifierLength;
constexpr std::size_t kInitialFlatBufferBytes = 512;  // ~header + a small entity

// enum mapping (compile-time checked)

constexpr std::uint8_t kEnumNone = 0;
constexpr std::uint8_t kEnumShip = 1;
constexpr std::uint8_t kEnumAsteroid = 2;
constexpr std::uint8_t kEnumBullet = 3;

static_assert(static_cast<std::uint8_t>(types::EntityType::None) == kEnumNone);
static_assert(static_cast<std::uint8_t>(types::EntityType::Ship) == kEnumShip);
static_assert(static_cast<std::uint8_t>(types::EntityType::Asteroid) == kEnumAsteroid);
static_assert(static_cast<std::uint8_t>(types::EntityType::Bullet) == kEnumBullet);

EntityType ToWire(types::EntityType t) noexcept {
    return static_cast<EntityType>(static_cast<std::uint8_t>(t));
}

types::EntityType FromWire(EntityType t) noexcept {
    const auto val = static_cast<std::uint8_t>(t);
    if (val > kEnumBullet) {
        return types::EntityType::None;
    }
    return static_cast<types::EntityType>(val);
}

}  // namespace

std::vector<std::uint8_t> SerializeEntity(const snapshot::EntitySnapshot& dto) {
    flatbuffers::FlatBufferBuilder fbb(kInitialFlatBufferBytes);

    // --- children first
    flatbuffers::Offset<Transform> transform_off = 0;
    if (dto.transform) {
        const auto& t = *dto.transform;
        const Vec2 pos{t.position.x, t.position.y};
        const Vec2 vel{t.velocity.x, t.velocity.y};
        transform_off = CreateTransform(fbb, &pos, &vel, t.angle);
    }

    flatbuffers::Offset<Ship> ship_off = 0;
    if (dto.ship) {
        const auto& s = *dto.ship;
        ship_off = CreateShip(fbb, fbb.CreateString(s.name), s.score, s.thrust_power, s.drag, s.is_thrusting, s.frags);
    }

    flatbuffers::Offset<Health> health_off = 0;
    if (dto.health)
        health_off = CreateHealth(fbb, dto.health->current, dto.health->max);

    flatbuffers::Offset<Combat> combat_off = 0;
    if (dto.combat)
        combat_off = CreateCombat(fbb, dto.combat->damage, dto.combat->owner_id);

    flatbuffers::Offset<Asteroid> asteroid_off = 0;
    if (dto.asteroid)
        asteroid_off = CreateAsteroid(fbb, dto.asteroid->score_reward);

    // --- root
    const auto root = CreateEntitySnapshot(fbb,
                                           dto.id,
                                           ToWire(dto.type),
                                           dto.mass,
                                           dto.radius,
                                           dto.is_destroyed,
                                           transform_off,
                                           ship_off,
                                           health_off,
                                           combat_off,
                                           asteroid_off);

    FinishEntitySnapshotBuffer(fbb, root);

    // TODO: zero-copy - return flatbuffers::DetachedBuffer
    const auto* p = fbb.GetBufferPointer();
    return {p, p + fbb.GetSize()};
}

std::optional<snapshot::EntitySnapshot> DeserializeEntity(std::span<const std::uint8_t> data) {
    if (data.empty())
        return std::nullopt;

    // 1) magic bytes fast-fail
    if (data.size() < kMinFlatBufferSize)
        return std::nullopt;
    if (!flatbuffers::BufferHasIdentifier(data.data(), EntitySnapshotIdentifier(), /*size_prefixed=*/false))
        return std::nullopt;

    // 2) structural verification (no identifier — already checked above)
    flatbuffers::Verifier verifier(data.data(), data.size());

    if (!VerifyEntitySnapshotBuffer(verifier))
        return std::nullopt;

    // 3) zero-copy root access
    const auto* fb = GetEntitySnapshot(data.data());

    assert(fb && "Verifier passed => non-null root");

    // 4) copy into the DTO
    snapshot::EntitySnapshot dto;
    dto.id = fb->id();
    dto.type = FromWire(fb->type());
    dto.mass = fb->mass();
    dto.radius = fb->radius();
    dto.is_destroyed = fb->is_destroyed();

    if (const auto* t = fb->transform()) {
        const auto* pos = t->position();
        const auto* vel = t->velocity();
        if (!pos || !vel) {
            // Malformed: Transform table present but required structs missing.
            return std::nullopt;
        }
        dto.transform = snapshot::TransformSnapshot{{pos->x(), pos->y()}, {vel->x(), vel->y()}, t->angle()};
    }
    if (const auto* s = fb->ship()) {
        dto.ship = snapshot::ShipSnapshot{s->name() ? s->name()->str() : std::string{},
                                          s->score(),
                                          s->thrust_power(),
                                          s->drag(),
                                          s->is_thrusting(),
                                          s->frags()};
    }
    if (const auto* h = fb->health())
        dto.health = snapshot::HealthSnapshot{h->current(), h->max()};
    if (const auto* c = fb->combat())
        dto.combat = snapshot::CombatSnapshot{c->damage(), c->owner_id()};
    if (const auto* a = fb->asteroid())
        dto.asteroid = snapshot::AsteroidSnapshot{a->score_reward()};

    return dto;
}

}  // namespace snapshot_wire

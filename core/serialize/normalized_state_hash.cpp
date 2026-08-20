#include "core/serialize/normalized_state_hash.h"

#include "core/world/region_movement.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace aetheria::serialize {
namespace {

inline constexpr std::uint64_t kFnvOffset = UINT64_C(14695981039346656037);
inline constexpr std::uint64_t kFnvPrime = UINT64_C(1099511628211);

void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= kFnvPrime;
}

template <typename Value, bool = std::is_enum_v<Value>> struct HashBits {
    using Type = Value;
};

template <typename Value> struct HashBits<Value, true> {
    using Type = std::underlying_type_t<Value>;
};

template <typename Value> void hash_scalar(std::uint64_t& hash, Value value) noexcept {
    static_assert(std::is_integral_v<Value> || std::is_enum_v<Value>);
    using Bits = typename HashBits<Value>::Type;
    using Unsigned = std::make_unsigned_t<Bits>;
    auto bits = static_cast<Unsigned>(static_cast<Bits>(value));
    for (std::size_t byte = 0; byte < sizeof(bits); ++byte) {
        hash_byte(hash, static_cast<std::uint8_t>(bits & UINT8_MAX));
        bits >>= 8U;
    }
}

void hash_string(std::uint64_t& hash, std::string_view value) noexcept {
    hash_scalar(hash, static_cast<std::uint64_t>(value.size()));
    for (const auto byte : value) {
        hash_byte(hash, static_cast<std::uint8_t>(byte));
    }
}

template <typename Value>
void hash_numeric_vector(std::uint64_t& hash, const std::vector<Value>& values) noexcept {
    hash_scalar(hash, static_cast<std::uint64_t>(values.size()));
    for (const auto value : values) {
        hash_scalar(hash, value);
    }
}

template <typename Id, typename Lookup>
void hash_definition_vector(std::uint64_t& hash, const std::vector<Id>& values, Lookup&& lookup,
                            std::string_view kind) {
    hash_scalar(hash, static_cast<std::uint64_t>(values.size()));
    for (const auto value : values) {
        const auto* definition = lookup(value);
        if (definition == nullptr) {
            throw std::runtime_error{"正規化雜湊遇到無效 " + std::string{kind}};
        }
        hash_string(hash, definition->id);
    }
}

template <typename Component> void require_stable_ids(const zone::Zone& zone) {
    for (const auto entity : zone.reg.view<const Component>()) {
        if (!zone.reg.all_of<world::StableId>(entity)) {
            throw std::runtime_error{"權威 unit component 缺少 StableId"};
        }
    }
}

}  // namespace

std::uint64_t normalized_state_hash(const zone::Zone& zone, const rules::Ruleset& ruleset) {
    auto hash = kFnvOffset;
    hash_scalar(hash, zone::value_of(zone.key));

    const auto clocks = zone.reg.view<const world::TurnClock>();
    if (clocks.size() > 1U) {
        throw std::runtime_error{"正規化雜湊至多允許一個 TurnClock"};
    }
    hash_scalar(hash, static_cast<std::uint64_t>(clocks.size()));
    if (!clocks.empty()) {
        hash_scalar(
            hash,
            static_cast<std::int64_t>(clocks.get<const world::TurnClock>(*clocks.begin()).now));
    }

    if (const auto* region = std::get_if<zone::RegionPayload>(&zone.payload)) {
        hash_scalar(hash, static_cast<std::uint64_t>(region->layers.size()));
        for (const auto& [z, tiles] : region->layers) {
            if (!tiles.valid_layout()) {
                throw std::runtime_error{"正規化雜湊遇到無效 RegionTiles layout"};
            }
            hash_scalar(hash, z);
            hash_scalar(hash, tiles.width);
            hash_scalar(hash, tiles.height);
            hash_definition_vector(
                hash, tiles.base, [&](rules::TerrainId id) { return ruleset.terrain(id); },
                "TerrainId");
            hash_definition_vector(
                hash, tiles.relief, [&](rules::ReliefId id) { return ruleset.relief(id); },
                "ReliefId");
            hash_definition_vector(
                hash, tiles.feature, [&](rules::FeatureId id) { return ruleset.feature(id); },
                "FeatureId");
            hash_numeric_vector(hash, tiles.temperature);
            hash_numeric_vector(hash, tiles.moisture);
            hash_numeric_vector(hash, tiles.elevation);
            hash_definition_vector(
                hash, tiles.edges, [&](rules::EdgeId id) { return ruleset.edge(id); }, "EdgeId");
            hash_scalar(hash, static_cast<std::uint64_t>(tiles.owner.size()));
            for (const auto owner : tiles.owner) {
                hash_scalar(hash, owner);
            }
            hash_numeric_vector(hash, tiles.settlement);
            hash_scalar(hash, static_cast<std::uint64_t>(tiles.portals.size()));
            for (const auto& portal : tiles.portals) {
                hash_scalar(hash, portal.tile.x);
                hash_scalar(hash, portal.tile.y);
                hash_scalar(hash, rules::value_of(portal.channel));
            }
            hash_scalar(hash, static_cast<std::uint64_t>(tiles.site.size()));
            for (const auto& site : tiles.site) {
                hash_scalar(hash, static_cast<std::uint8_t>(site.ever_realized));
            }
        }
    } else {
        hash_scalar(hash, static_cast<std::uint64_t>(zone.payload.index()));
    }

    require_stable_ids<world::RegionPosition>(zone);
    require_stable_ids<world::MovementPoints>(zone);
    require_stable_ids<world::RegionMoveCommand>(zone);
    std::vector<std::pair<std::uint64_t, entt::entity>> entities;
    for (const auto entity : zone.reg.view<const world::StableId>()) {
        entities.emplace_back(zone.reg.get<const world::StableId>(entity).uid, entity);
    }
    std::ranges::sort(entities);
    hash_scalar(hash, static_cast<std::uint64_t>(entities.size()));
    std::uint64_t previous{};
    bool has_previous{};
    for (const auto [stable_id, entity] : entities) {
        if (has_previous && stable_id == previous) {
            throw std::runtime_error{"正規化雜湊遇到重複 StableId：" + std::to_string(stable_id)};
        }
        previous = stable_id;
        has_previous = true;
        hash_scalar(hash, stable_id);

        const auto* position = zone.reg.try_get<const world::RegionPosition>(entity);
        hash_scalar(hash, static_cast<std::uint8_t>(position != nullptr));
        if (position != nullptr) {
            hash_scalar(hash, position->z);
            hash_scalar(hash, position->tile.x);
            hash_scalar(hash, position->tile.y);
        }
        const auto* points = zone.reg.try_get<const world::MovementPoints>(entity);
        hash_scalar(hash, static_cast<std::uint8_t>(points != nullptr));
        if (points != nullptr) {
            hash_scalar(hash, points->current);
            hash_scalar(hash, points->per_xun);
        }
        const auto* command = zone.reg.try_get<const world::RegionMoveCommand>(entity);
        hash_scalar(hash, static_cast<std::uint8_t>(command != nullptr));
        if (command != nullptr) {
            hash_scalar(hash, command->target.x);
            hash_scalar(hash, command->target.y);
            hash_scalar(hash, static_cast<std::uint8_t>(command->collected));
        }
    }
    return hash;
}

}  // namespace aetheria::serialize

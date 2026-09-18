// dungeon_validation.cpp：地城內容驗證與確定性雜湊。

#include "core/local/dungeon.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "core/rules/power.h"
#include "core/worldgen/region_seed.h"

namespace aetheria::local {
namespace {

constexpr std::uint64_t kFnvOffset = UINT64_C(14695981039346656037);
constexpr std::uint64_t kFnvPrime = UINT64_C(1099511628211);

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
    using Raw = typename HashBits<Value>::Type;
    using Unsigned = std::make_unsigned_t<Raw>;
    auto bits = static_cast<Unsigned>(static_cast<Raw>(value));
    for (std::size_t index = 0; index < sizeof(bits); ++index) {
        hash_byte(hash, static_cast<std::uint8_t>(bits & UINT8_MAX));
        bits >>= 8U;
    }
}

void hash_string(std::uint64_t& hash, std::string_view value) noexcept {
    hash_scalar(hash, static_cast<std::uint64_t>(value.size()));
    for (const auto character : value) {
        hash_byte(hash, static_cast<std::uint8_t>(character));
    }
}

}  // namespace

bool valid_dungeon(const DungeonGenerated& dungeon, const rules::Ruleset& ruleset) noexcept {
    if (dungeon.uid == 0 || dungeon.floors.empty() || dungeon.entrance_clue.clue_key.empty() ||
        dungeon.boss.uid == 0 || dungeon.boss.significance < world::Significance::Site ||
        ruleset.breakthrough(dungeon.boss.required_breakthrough) == nullptr) {
        return false;
    }
    std::int32_t previous = std::numeric_limits<std::int32_t>::min();
    for (std::size_t index = 0; index < dungeon.floors.size(); ++index) {
        const auto& floor = dungeon.floors[index];
        if (floor.depth != index + 1U || floor.difficulty <= previous || floor.rooms.size() < 2U ||
            floor.corridor_count + 1U != floor.rooms.size() || floor.light_cost == 0 ||
            floor.retreat_cost == 0) {
            return false;
        }
        previous = floor.difficulty;
        for (const auto& trap : floor.traps) {
            if (trap.uid == 0 || trap.depth != floor.depth || ruleset.trap(trap.definition) == nullptr ||
                trap.tile.x >= kLocalWidth || trap.tile.y >= kLocalHeight) {
                return false;
            }
        }
        for (const auto& treasure : floor.treasures) {
            if (treasure.uid == 0 || treasure.value == 0 ||
                (treasure.origin.has_value() != treasure.unique) ||
                (treasure.breakthrough.has_value() &&
                 ruleset.breakthrough(*treasure.breakthrough) == nullptr)) {
                return false;
            }
        }
    }
    return true;
}

std::uint64_t hash_dungeon(const DungeonGenerated& dungeon) noexcept {
    auto hash = kFnvOffset;
    hash_scalar(hash, dungeon.uid);
    hash_scalar(hash, dungeon.archetype);
    hash_scalar(hash, static_cast<std::uint8_t>(dungeon.cleared));
    hash_string(hash, dungeon.entrance_clue.clue_key);
    hash_scalar(hash, dungeon.entrance_clue.predicted_deepest_difficulty);
    for (const auto& floor : dungeon.floors) {
        hash_scalar(hash, floor.depth);
        hash_scalar(hash, floor.difficulty);
        hash_scalar(hash, floor.enemy_tier);
        hash_scalar(hash, floor.enemy_count);
        hash_scalar(hash, floor.light_cost);
        hash_scalar(hash, floor.retreat_cost);
        for (const auto& room : floor.rooms) {
            hash_scalar(hash, room.origin.x);
            hash_scalar(hash, room.origin.y);
            hash_scalar(hash, room.width);
            hash_scalar(hash, room.height);
            hash_scalar(hash, static_cast<std::uint8_t>(room.natural));
            hash_scalar(hash, static_cast<std::uint8_t>(room.eroded));
        }
        for (const auto& trap : floor.traps) {
            hash_scalar(hash, trap.uid);
            hash_scalar(hash, rules::value_of(trap.definition));
            hash_scalar(hash, static_cast<std::uint8_t>(trap.triggered));
        }
        for (const auto& treasure : floor.treasures) {
            hash_scalar(hash, treasure.uid);
            hash_scalar(hash, treasure.value);
            hash_scalar(hash, static_cast<std::uint8_t>(treasure.unique));
            hash_scalar(hash, static_cast<std::uint8_t>(treasure.origin.has_value()));
            if (treasure.origin.has_value()) {
                hash_scalar(hash, treasure.origin->event_id);
                hash_scalar(hash, treasure.origin->region_id);
                hash_scalar(hash, treasure.origin->ancient_site_canonical_id);
                hash_scalar(hash, treasure.origin->ancient_site_tier);
                hash_scalar(hash, static_cast<std::uint8_t>(treasure.origin->survived_cataclysm));
            }
            hash_scalar(hash, static_cast<std::uint8_t>(treasure.breakthrough.has_value()));
            if (treasure.breakthrough.has_value()) {
                hash_scalar(hash, rules::value_of(*treasure.breakthrough));
            }
        }
    }
    hash_scalar(hash, dungeon.boss.uid);
    hash_scalar(hash, dungeon.boss.significance);
    hash_scalar(hash, dungeon.boss.combat_tier);
    hash_scalar(hash, rules::value_of(dungeon.boss.required_breakthrough));
    hash_scalar(hash, static_cast<std::uint8_t>(dungeon.boss.alive));
    return hash;
}

}  // namespace aetheria::local

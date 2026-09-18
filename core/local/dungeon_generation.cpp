// dungeon_generation.cpp：依規則生成各層敵人、機關、寶藏與 Boss。

#include "core/local/dungeon.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "core/rules/power.h"
#include "core/worldgen/region_seed.h"

#include "core/local/dungeon_detail.h"

namespace aetheria::local {
using detail::dungeon::contains_uid;
using detail::dungeon::generate_rooms;

namespace {

constexpr std::uint64_t kDungeonUidSalt = UINT64_C(0x44554E47454F4E31);

[[nodiscard]] std::uint16_t scaled_for_cleared(std::uint32_t value, bool cleared,
                                               const rules::DungeonRules& rules) noexcept {
    if (!cleared) {
        return static_cast<std::uint16_t>(
            std::min<std::uint32_t>(value, std::numeric_limits<std::uint16_t>::max()));
    }
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(
        value * rules.cleared_density_numerator / rules.cleared_density_denominator,
        std::numeric_limits<std::uint16_t>::max()));
}

[[nodiscard]] world::Significance enemy_tier(std::int32_t difficulty) noexcept {
    if (difficulty >= 90) return world::Significance::Region;
    if (difficulty >= 68) return world::Significance::Site;
    if (difficulty >= 44) return world::Significance::Local;
    return world::Significance::Ambient;
}


[[nodiscard]] std::uint64_t content_uid(std::uint64_t dungeon_uid, std::uint8_t depth,
                                        std::uint16_t index, std::uint64_t salt) noexcept {
    return worldgen::splitmix64(dungeon_uid ^ salt ^
                                (static_cast<std::uint64_t>(depth) << 40U) ^ index);
}

}  // namespace

DungeonGenerated generate_dungeon(std::uint64_t seed, rules::DungeonArchetype archetype,
                                  const site::PersistentDungeon& persistent,
                                  const DungeonPersistentState& local_persistent,
                                  const rules::Ruleset& ruleset,
                                  const std::optional<DungeonHistoryOrigin>& history_origin) {
    const auto& rules = ruleset.dungeon_rules();
    if (!rules.loaded || ruleset.traps().empty() || !valid_dungeon_persistent_state(local_persistent)) {
        throw std::invalid_argument{"地城生成要求有效 Ruleset 與 Local 持久層"};
    }
    if (persistent.uid == 0 || persistent.place_name_key.empty() || persistent.depth == 0 ||
        persistent.depth > 64U || rules::archetype_index(archetype) >= rules.archetypes.size()) {
        throw std::invalid_argument{"地城生成收到無效 Site PersistentDungeon"};
    }
    const auto artifact = ruleset.find_breakthrough("breakthrough.artifact");
    if (!artifact.has_value()) {
        throw std::runtime_error{"地城 Boss 缺少既有 breakthrough.artifact"};
    }

    DungeonGenerated result;
    result.uid = worldgen::splitmix64(seed ^ persistent.uid ^ kDungeonUidSalt);
    result.archetype = archetype;
    result.cleared = persistent.cleared;
    const auto rating = static_cast<std::int32_t>(worldgen::splitmix64(seed) % 31U);
    const auto deepest = rules.difficulty_base + rating +
                         rules.difficulty_depth_step * persistent.depth;
    const auto noise_span = static_cast<std::uint64_t>(rules.clue_noise * 2 + 1);
    const auto noise = static_cast<std::int32_t>(worldgen::splitmix64(seed ^ UINT64_C(0xC1)) %
                                                 noise_span) -
                       rules.clue_noise;
    result.entrance_clue = {"dungeon.clue.equipment_and_remains", deepest + noise};

    const auto& profile = rules.archetypes[rules::archetype_index(archetype)];
    result.floors.reserve(persistent.depth);
    for (std::uint16_t level = 1; level <= persistent.depth; ++level) {
        const auto depth = static_cast<std::uint8_t>(level);
        DungeonFloor floor;
        floor.depth = depth;
        floor.difficulty = rules.difficulty_base + rating + rules.difficulty_depth_step * depth;
        floor.enemy_tier = enemy_tier(floor.difficulty);
        floor.enemy_count = scaled_for_cleared(
            static_cast<std::uint32_t>(rules.enemy_base) +
                static_cast<std::uint32_t>(rules.enemy_depth_step) * depth +
                profile.guardian_weight / 3U,
            persistent.cleared, rules);
        floor.light_cost = static_cast<std::uint16_t>(
            rules.light_base_cost + rules.light_depth_step * depth);
        floor.retreat_cost = static_cast<std::uint16_t>(depth * (depth + 1U) / 2U);
        floor.rooms = generate_rooms(seed, depth, profile);
        floor.corridor_count = static_cast<std::uint16_t>(floor.rooms.size() - 1U);

        const auto trap_count = static_cast<std::uint16_t>(
            1U + static_cast<std::uint32_t>(profile.trap_weight) * depth / 6U);
        floor.traps.reserve(trap_count);
        for (std::uint16_t index = 0; index < trap_count; ++index) {
            const auto uid = content_uid(result.uid, depth, index, UINT64_C(0x54524150));
            const auto random = worldgen::splitmix64(uid);
            floor.traps.push_back({
                .uid = uid,
                .definition = static_cast<rules::TrapDefId>(random % ruleset.traps().size()),
                .tile = {static_cast<std::uint16_t>(2U + (random >> 16U) % 60U),
                         static_cast<std::uint16_t>(2U + (random >> 32U) % 60U)},
                .depth = depth,
                .triggered = contains_uid(local_persistent.triggered_trap_uids, uid),
            });
        }

        const auto treasure_uid = content_uid(result.uid, depth, 0, UINT64_C(0x4C4F4F54));
        if (!contains_uid(local_persistent.claimed_treasure_uids, treasure_uid)) {
            const auto value = scaled_for_cleared(
                static_cast<std::uint32_t>(rules.treasure_base) +
                    static_cast<std::uint32_t>(rules.treasure_depth_step) * depth,
                persistent.cleared, rules);
            if (value > 0) {
                floor.treasures.push_back({treasure_uid, value, false, std::nullopt, std::nullopt});
            }
        }
        if (level == persistent.depth) {
            const auto unique_uid = content_uid(result.uid, depth, 1, UINT64_C(0x554E4951));
            if (!contains_uid(local_persistent.claimed_treasure_uids, unique_uid)) {
                const auto value = scaled_for_cleared(
                    static_cast<std::uint32_t>(rules.treasure_base) +
                        static_cast<std::uint32_t>(rules.treasure_depth_step) * depth * 3U,
                    persistent.cleared, rules);
                if (value > 0) {
                    floor.treasures.push_back(
                        {unique_uid, value, history_origin.has_value(), history_origin, *artifact});
                }
            }
        }
        result.floors.push_back(std::move(floor));
    }
    result.boss = {content_uid(result.uid, static_cast<std::uint8_t>(persistent.depth), 0,
                               UINT64_C(0x424F5353)),
                   world::Significance::Region, world::Significance::Region, *artifact, true};
    if (!valid_dungeon(result, ruleset)) {
        throw std::logic_error{"共用地城生成器產生無效深度曲線或內容"};
    }
    return result;
}

DungeonGenerated generate_dungeon(
    std::uint64_t seed, const site::PersistentDungeon& persistent,
    const DungeonPersistentState& local_persistent, const rules::Ruleset& ruleset,
    const std::optional<DungeonHistoryOrigin>& history_origin) {
    return generate_dungeon(seed, rules::DungeonArchetype::Hybrid, persistent, local_persistent,
                            ruleset, history_origin);
}

}  // namespace aetheria::local

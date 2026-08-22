// dungeon.cpp：單一路徑生成三種地城內容，並實作機關、Boss 與持久資源互動。

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

constexpr std::uint64_t kDungeonUidSalt = UINT64_C(0x44554E47454F4E31);
constexpr std::uint64_t kHistoryEventSalt = UINT64_C(0x4849535444554E31);
constexpr std::uint64_t kFnvOffset = UINT64_C(14695981039346656037);
constexpr std::uint64_t kFnvPrime = UINT64_C(1099511628211);

[[nodiscard]] std::uint64_t historical_event_id(std::uint32_t region_id,
                                                std::uint32_t canonical_id,
                                                bool survived) noexcept {
    const auto identity = (static_cast<std::uint64_t>(region_id) << 32U) | canonical_id;
    return worldgen::splitmix64(kHistoryEventSalt ^ identity ^
                                (survived ? UINT64_C(1) << 63U : UINT64_C(0)));
}

[[nodiscard]] const worldgen::CitySite* ancient_site(
    std::uint32_t canonical_id, const worldgen::HistoryStageOutput& history) noexcept {
    const auto found = std::ranges::find(history.ancient_sites.cities, canonical_id,
                                         &worldgen::CitySite::canonical_id);
    return found == history.ancient_sites.cities.end() ? nullptr : &*found;
}

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

[[nodiscard]] bool contains_uid(const std::vector<std::uint64_t>& values,
                                std::uint64_t uid) noexcept {
    return std::ranges::binary_search(values, uid);
}

void insert_uid(std::vector<std::uint64_t>& values, std::uint64_t uid) {
    const auto position = std::ranges::lower_bound(values, uid);
    if (position == values.end() || *position != uid) {
        values.insert(position, uid);
    }
}

[[nodiscard]] std::int32_t attribute_value(const rules::Attributes& attributes,
                                           rules::TrapCheckAttribute attribute) noexcept {
    switch (attribute) {
        case rules::TrapCheckAttribute::Skill:
            return attributes.skill;
        case rules::TrapCheckAttribute::Mind:
            return attributes.mind;
        case rules::TrapCheckAttribute::Spirit:
            return attributes.spirit;
    }
    return 0;
}

[[nodiscard]] world::Significance enemy_tier(std::int32_t difficulty) noexcept {
    if (difficulty >= 90) return world::Significance::Region;
    if (difficulty >= 68) return world::Significance::Site;
    if (difficulty >= 44) return world::Significance::Local;
    return world::Significance::Ambient;
}

[[nodiscard]] std::vector<DungeonRoom> generate_rooms(
    std::uint64_t seed, std::uint8_t depth, const rules::DungeonArchetypeRules& profile) {
    std::vector<DungeonRoom> rooms;
    rooms.reserve(profile.room_count);
    for (std::uint8_t index = 0; index < profile.room_count; ++index) {
        const auto random = worldgen::splitmix64(
            seed ^ (static_cast<std::uint64_t>(depth) << 48U) ^
            (static_cast<std::uint64_t>(index) << 32U));
        const auto width = static_cast<std::uint8_t>(6U + random % 5U);
        const auto height = static_cast<std::uint8_t>(6U + (random >> 8U) % 5U);
        const bool mirrored = index % 2U != 0U && !rooms.empty() &&
                              (random >> 16U) % 100U < profile.symmetry_percent;
        LocalXY origin;
        if (mirrored) {
            const auto& previous = rooms.back();
            origin.x = static_cast<std::uint16_t>(kLocalWidth - previous.origin.x - width);
            origin.y = previous.origin.y;
        } else {
            origin.x = static_cast<std::uint16_t>(2U + (random >> 24U) % (60U - width));
            origin.y = static_cast<std::uint16_t>(2U + (random >> 40U) % (60U - height));
        }
        rooms.push_back({
            .origin = origin,
            .width = width,
            .height = height,
            .natural = (random >> 20U) % 100U < profile.natural_percent,
            .eroded = (random >> 52U) % 100U < profile.erosion_percent,
        });
    }
    return rooms;
}

[[nodiscard]] std::uint64_t content_uid(std::uint64_t dungeon_uid, std::uint8_t depth,
                                        std::uint16_t index, std::uint64_t salt) noexcept {
    return worldgen::splitmix64(dungeon_uid ^ salt ^
                                (static_cast<std::uint64_t>(depth) << 40U) ^ index);
}

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

bool valid_dungeon_persistent_state(const DungeonPersistentState& state) noexcept {
    const auto valid = [](const auto& values) {
        return std::ranges::is_sorted(values) &&
               std::ranges::adjacent_find(values) == values.end() &&
               std::ranges::none_of(values, [](std::uint64_t uid) { return uid == 0; });
    };
    return valid(state.triggered_trap_uids) && valid(state.claimed_treasure_uids);
}

std::uint8_t choose_dungeon_depth(DungeonDepthSource source, std::uint64_t seed,
                                  std::uint8_t story_depth) {
    switch (source) {
        case DungeonDepthSource::BeastLair:
            return static_cast<std::uint8_t>(1U + seed % 2U);
        case DungeonDepthSource::Mine:
            return static_cast<std::uint8_t>(3U + seed % 4U);
        case DungeonDepthSource::Ruin:
            return static_cast<std::uint8_t>(5U + seed % 6U);
        case DungeonDepthSource::Story:
            if (story_depth == 0 || story_depth > 64U) {
                throw std::invalid_argument{"劇情地城深度必須介於 1 與 64"};
            }
            return story_depth;
    }
    throw std::invalid_argument{"未知地城深度來源"};
}

std::optional<DungeonHistoryOrigin> dungeon_history_origin(
    std::uint32_t region_id, std::uint32_t ancient_site_canonical_id,
    const worldgen::HistoryStageOutput& history) noexcept {
    const auto* site = ancient_site(ancient_site_canonical_id, history);
    if (site == nullptr || ancient_site_canonical_id >= history.survivor.size()) {
        return std::nullopt;
    }
    const bool survived = history.survivor[ancient_site_canonical_id] != 0;
    return DungeonHistoryOrigin{
        historical_event_id(region_id, ancient_site_canonical_id, survived), region_id,
        ancient_site_canonical_id, site->tier, survived};
}

bool origin_matches_history(const DungeonHistoryOrigin& origin,
                            const worldgen::HistoryStageOutput& history) noexcept {
    const auto expected = dungeon_history_origin(origin.region_id,
                                                 origin.ancient_site_canonical_id, history);
    return expected.has_value() && *expected == origin;
}

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

rules::CheckResult detect_trap(const DungeonTrap& trap, std::uint8_t roll,
                               const rules::Attributes& attributes,
                               const rules::Ruleset& ruleset, bool has_light) {
    const auto* definition = ruleset.trap(trap.definition);
    if (definition == nullptr) {
        throw std::invalid_argument{"偵測機關收到無效 TrapDefId"};
    }
    return rules::evaluate_check(roll, attribute_value(attributes, definition->detection_attribute),
                                 dungeon_light_effect(has_light, ruleset).detection_modifier,
                                 definition->detection_difficulty + trap.depth * 2,
                                 ruleset.check_rules());
}

std::optional<rules::CheckResult> disarm_trap(const DungeonTrap& trap, std::uint8_t roll,
                                              const rules::Attributes& attributes,
                                              const rules::Ruleset& ruleset) {
    const auto* definition = ruleset.trap(trap.definition);
    if (definition == nullptr) {
        throw std::invalid_argument{"解除機關收到無效 TrapDefId"};
    }
    if (definition->disarm_method != rules::TrapDisarmMethod::Attribute) {
        return std::nullopt;
    }
    return rules::evaluate_check(roll, attribute_value(attributes, definition->disarm_attribute),
                                 0, definition->disarm_difficulty + trap.depth * 2,
                                 ruleset.check_rules());
}

TrapTriggerResult trigger_trap(const DungeonTrap& trap, std::uint64_t target_uid,
                               bool target_is_enemy, DungeonPersistentState& persistent,
                               const rules::Ruleset& ruleset) {
    const auto* definition = ruleset.trap(trap.definition);
    if (definition == nullptr || target_uid == 0 || !valid_dungeon_persistent_state(persistent)) {
        throw std::invalid_argument{"觸發機關收到無效 TrapDef、目標或持久層"};
    }
    const bool newly_triggered = !contains_uid(persistent.triggered_trap_uids, trap.uid);
    insert_uid(persistent.triggered_trap_uids, trap.uid);
    return {target_uid, target_is_enemy,
            static_cast<std::int64_t>(definition->base_damage) +
                static_cast<std::int64_t>(definition->damage_per_depth) * trap.depth,
            newly_triggered};
}

std::int64_t boss_damage(const DungeonBoss& boss, std::int64_t proposed_damage,
                         world::Significance attacker_tier, const rules::Ruleset& ruleset,
                         const rules::PowerBreakthroughDef* breakthrough) {
    if (!boss.alive) {
        throw std::invalid_argument{"不能攻擊已擊敗的地城 Boss"};
    }
    return rules::apply_individual_tier_gate(proposed_damage, attacker_tier, boss.combat_tier,
                                             true, ruleset.power_rules(), breakthrough);
}

DungeonBossDefeatEvent defeat_boss(DungeonBoss& boss, std::uint64_t dungeon_uid) {
    if (!boss.alive || dungeon_uid == 0) {
        throw std::invalid_argument{"地城 Boss 擊敗事件狀態無效"};
    }
    boss.alive = false;
    return {dungeon_uid, world::Significance::Region, "event.dungeon_boss_defeated"};
}

std::optional<std::uint8_t> light_exhaustion_depth(const DungeonGenerated& dungeon,
                                                   std::uint32_t light_supply) noexcept {
    std::uint64_t spent{};
    for (const auto& floor : dungeon.floors) {
        spent += floor.light_cost;
        if (spent > light_supply) {
            return floor.depth;
        }
    }
    return std::nullopt;
}

DungeonLightEffect dungeon_light_effect(bool has_light,
                                        const rules::Ruleset& ruleset) noexcept {
    const auto& rules = ruleset.dungeon_rules();
    if (has_light) {
        return {rules.lit_vision, 0, 0};
    }
    return {rules.unlit_vision, rules.unlit_hit_modifier, rules.unlit_detection_modifier};
}

std::uint64_t available_treasure_value(const DungeonGenerated& dungeon) noexcept {
    std::uint64_t result{};
    for (const auto& floor : dungeon.floors) {
        for (const auto& treasure : floor.treasures) {
            result += treasure.value;
        }
    }
    return result;
}

void claim_all_treasure(const DungeonGenerated& dungeon, DungeonPersistentState& persistent) {
    if (!valid_dungeon_persistent_state(persistent)) {
        throw std::invalid_argument{"領取寶藏收到無效持久層"};
    }
    for (const auto& floor : dungeon.floors) {
        for (const auto& treasure : floor.treasures) {
            insert_uid(persistent.claimed_treasure_uids, treasure.uid);
        }
    }
}

}  // namespace aetheria::local

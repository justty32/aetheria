#pragma once

// dungeon.h：L3 地城共用生成器、機關互動、Boss 階差循環與資源壓力。
// 玩法核心只依賴純 C++ 規則；Godot 不持有地城真值。

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/local/dungeon_state.h"
#include "core/local/local_tiles.h"
#include "core/rules/attributes.h"
#include "core/rules/check.h"
#include "core/rules/dungeon_rules.h"
#include "core/rules/ruleset.h"
#include "core/site/site_projection.h"
#include "core/world/significance.h"
#include "core/world/region_tiles.h"
#include "core/worldgen/region_civ_stages.h"

namespace aetheria::local {

enum class DungeonDepthSource : std::uint8_t {
    BeastLair,
    Mine,
    Ruin,
    Story,
};

struct DungeonHistoryOrigin {
    std::uint64_t event_id{};
    std::uint32_t region_id{};
    std::uint32_t ancient_site_canonical_id{};
    world::SettlementTier ancient_site_tier{world::SettlementTier::None};
    bool survived_cataclysm{};

    constexpr bool operator==(const DungeonHistoryOrigin&) const noexcept = default;
};

struct DungeonRoom {
    LocalXY origin;
    std::uint8_t width{};
    std::uint8_t height{};
    bool natural{};
    bool eroded{};

    constexpr bool operator==(const DungeonRoom&) const noexcept = default;
};

struct DungeonTrap {
    std::uint64_t uid{};
    rules::TrapDefId definition{};
    LocalXY tile;
    std::uint8_t depth{};
    bool triggered{};

    constexpr bool operator==(const DungeonTrap&) const noexcept = default;
};

struct DungeonTreasure {
    std::uint64_t uid{};
    std::uint32_t value{};
    bool unique{};
    std::optional<DungeonHistoryOrigin> origin;
    std::optional<rules::PowerBreakthroughDefId> breakthrough;

    bool operator==(const DungeonTreasure&) const = default;
};

struct DungeonFloor {
    std::uint8_t depth{};
    std::int32_t difficulty{};
    world::Significance enemy_tier{world::Significance::Ambient};
    std::uint16_t enemy_count{};
    std::uint16_t light_cost{};
    std::uint16_t retreat_cost{};
    std::vector<DungeonRoom> rooms;
    std::uint16_t corridor_count{};
    std::vector<DungeonTrap> traps;
    std::vector<DungeonTreasure> treasures;

    bool operator==(const DungeonFloor&) const = default;
};

struct DungeonEntranceClue {
    std::string clue_key;
    std::int32_t predicted_deepest_difficulty{};

    bool operator==(const DungeonEntranceClue&) const = default;
};

struct DungeonBoss {
    std::uint64_t uid{};
    world::Significance significance{world::Significance::Region};
    world::Significance combat_tier{world::Significance::Region};
    rules::PowerBreakthroughDefId required_breakthrough{};
    bool alive{true};

    constexpr bool operator==(const DungeonBoss&) const noexcept = default;
};

struct DungeonGenerated {
    std::uint64_t uid{};
    rules::DungeonArchetype archetype{rules::DungeonArchetype::Hybrid};
    bool cleared{};
    DungeonEntranceClue entrance_clue;
    std::vector<DungeonFloor> floors;
    DungeonBoss boss;

    bool operator==(const DungeonGenerated&) const = default;
};

struct TrapTriggerResult {
    std::uint64_t target_uid{};
    bool target_was_enemy{};
    std::int64_t damage{};
    bool newly_triggered{};
};

struct DungeonBossDefeatEvent {
    std::uint64_t dungeon_uid{};
    world::Significance significance{world::Significance::Region};
    std::string event_key;
};

struct DungeonLightEffect {
    std::uint8_t vision{};
    std::int32_t hit_modifier{};
    std::int32_t detection_modifier{};

    constexpr bool operator==(const DungeonLightEffect&) const noexcept = default;
};

[[nodiscard]] std::uint8_t choose_dungeon_depth(DungeonDepthSource source, std::uint64_t seed,
                                                std::uint8_t story_depth = 0);
[[nodiscard]] std::optional<DungeonHistoryOrigin> dungeon_history_origin(
    std::uint32_t region_id, std::uint32_t ancient_site_canonical_id,
    const worldgen::HistoryStageOutput& history) noexcept;
[[nodiscard]] bool origin_matches_history(const DungeonHistoryOrigin& origin,
                                          const worldgen::HistoryStageOutput& history) noexcept;

[[nodiscard]] DungeonGenerated generate_dungeon(
    std::uint64_t seed, rules::DungeonArchetype archetype,
    const site::PersistentDungeon& persistent, const DungeonPersistentState& local_persistent,
    const rules::Ruleset& ruleset,
    const std::optional<DungeonHistoryOrigin>& history_origin = std::nullopt);
[[nodiscard]] DungeonGenerated generate_dungeon(
    std::uint64_t seed, const site::PersistentDungeon& persistent,
    const DungeonPersistentState& local_persistent, const rules::Ruleset& ruleset,
    const std::optional<DungeonHistoryOrigin>& history_origin = std::nullopt);
[[nodiscard]] bool valid_dungeon(const DungeonGenerated& dungeon,
                                 const rules::Ruleset& ruleset) noexcept;
[[nodiscard]] std::uint64_t hash_dungeon(const DungeonGenerated& dungeon) noexcept;

[[nodiscard]] rules::CheckResult detect_trap(const DungeonTrap& trap, std::uint8_t roll,
                                             const rules::Attributes& attributes,
                                             const rules::Ruleset& ruleset,
                                             bool has_light = true);
[[nodiscard]] std::optional<rules::CheckResult> disarm_trap(
    const DungeonTrap& trap, std::uint8_t roll, const rules::Attributes& attributes,
    const rules::Ruleset& ruleset);
[[nodiscard]] TrapTriggerResult trigger_trap(const DungeonTrap& trap, std::uint64_t target_uid,
                                             bool target_is_enemy,
                                             DungeonPersistentState& persistent,
                                             const rules::Ruleset& ruleset);

[[nodiscard]] std::int64_t boss_damage(const DungeonBoss& boss, std::int64_t proposed_damage,
                                       world::Significance attacker_tier,
                                       const rules::Ruleset& ruleset,
                                       const rules::PowerBreakthroughDef* breakthrough = nullptr);
[[nodiscard]] DungeonBossDefeatEvent defeat_boss(DungeonBoss& boss,
                                                  std::uint64_t dungeon_uid);

[[nodiscard]] std::optional<std::uint8_t> light_exhaustion_depth(
    const DungeonGenerated& dungeon, std::uint32_t light_supply) noexcept;
[[nodiscard]] DungeonLightEffect dungeon_light_effect(bool has_light,
                                                      const rules::Ruleset& ruleset) noexcept;
[[nodiscard]] std::uint64_t available_treasure_value(const DungeonGenerated& dungeon) noexcept;
void claim_all_treasure(const DungeonGenerated& dungeon, DungeonPersistentState& persistent);

}  // namespace aetheria::local

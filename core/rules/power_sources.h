#pragma once

// core/rules/power_sources.h：魔法、信仰、血統共用的位階來源接點，
// 以及三層魔法、root 神祇事件與種族上限的純 C++ 規則。

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/rules/attributes.h"
#include "core/rules/check.h"
#include "core/world/significance.h"

namespace aetheria::rules {

enum class SchoolDefId : std::uint16_t {};
enum class TenetDefId : std::uint16_t {};
enum class DeityDefId : std::uint16_t {};
enum class RaceDefId : std::uint16_t {};

[[nodiscard]] constexpr std::uint16_t value_of(SchoolDefId id) noexcept {
    return static_cast<std::uint16_t>(id);
}
[[nodiscard]] constexpr std::uint16_t value_of(TenetDefId id) noexcept {
    return static_cast<std::uint16_t>(id);
}
[[nodiscard]] constexpr std::uint16_t value_of(DeityDefId id) noexcept {
    return static_cast<std::uint16_t>(id);
}
[[nodiscard]] constexpr std::uint16_t value_of(RaceDefId id) noexcept {
    return static_cast<std::uint16_t>(id);
}

enum class PowerSourceKind : std::uint8_t { Magic, Faith, Bloodline };

// 三條來源都必須先轉成這個形狀，resolve_power_profile 才會接到位階與屬性。
struct PowerSourceDef {
    PowerSourceKind kind{PowerSourceKind::Magic};
    std::string origin_id;
    std::int8_t tier_bonus{};
    std::vector<std::string> ability_ids;
    Attributes attribute_modifier;
    std::string removal_condition;
};

struct PowerSourceState {
    const PowerSourceDef* definition{};
    bool available{true};
    std::string unavailable_reason;
};

struct PowerProfile {
    world::Significance tier{world::Significance::Ambient};
    Attributes attributes;
    std::vector<std::string> ability_ids;
};

// 唯一的來源彙整接點；位階加成最後一律受血統上限約束。
[[nodiscard]] PowerProfile resolve_power_profile(
    world::Significance base_tier, world::Significance tier_cap, Attributes base_attributes,
    std::span<const PowerSourceState> sources);

struct SchoolDef {
    std::string id;
    std::string name_key;
    PowerSourceDef source;
};

struct TenetDef {
    std::string id;
    std::string name_key;
    std::vector<std::string> conflicts;
};

struct DeityDef {
    std::string id;
    std::string name_key;
    world::Significance significance{world::Significance::World};
    std::vector<std::string> domains;
    std::vector<std::string> tenets;
    std::vector<std::string> hostile_deities;
    PowerSourceDef source;
};

struct RaceDef {
    std::string id;
    std::string name_key;
    PowerSourceDef source;
    world::Significance tier_cap{world::Significance::Ambient};
    std::uint32_t lifespan_years{};
};

struct PowerSourceRules {
    std::int32_t strategic_minimum_resource_cost{};
    std::int32_t strategic_minimum_ritual_xun{};
    std::int32_t strategic_minimum_casters{};
    std::int32_t strategic_minimum_cooldown_xun{};
    std::int32_t strategic_reference_resource_income_per_xun{};
    bool loaded{};
};

enum class SpellScale : std::uint8_t { Region, Site, Local };

struct StrategicSpellDef {
    std::string id;
    SpellScale scale{SpellScale::Region};
    std::int32_t magical_resource_cost{};
    std::int32_t ritual_duration_xun{};
    std::int32_t required_casters{};
    std::int32_t cooldown_xun{};
    std::int32_t weather_delta{};
    std::int32_t combat_posture_delta{};
};

struct StrategicRitual {
    std::int32_t remaining_xun{};
    std::int32_t cooldown_xun{};
    std::int32_t weather_delta{};
    std::int32_t combat_posture_delta{};
};

struct RegionMagicState {
    std::int32_t magical_resource{};
    std::int32_t resource_income_per_xun{};
    std::int32_t weather_modifier{};
    std::int32_t combat_posture_modifier{};
    std::int32_t cooldown_remaining_xun{};
    std::vector<StrategicRitual> rituals;
};

[[nodiscard]] bool begin_strategic_ritual(RegionMagicState& region,
                                          const StrategicSpellDef& spell,
                                          std::int32_t available_casters,
                                          const PowerSourceRules& rules);
void advance_region_magic(RegionMagicState& region, std::int32_t elapsed_xun);

struct TacticalSpellDef {
    std::string id;
    SpellScale scale{SpellScale::Site};
    std::int32_t mana_cost{};
    std::int32_t attack_modifier{};
    std::int32_t defense_modifier{};
    std::int32_t summoned_mass{};
};

struct FormationMagicState {
    std::int32_t attack_modifier{};
    std::int32_t defense_modifier{};
    std::int32_t summoned_mass{};
};

[[nodiscard]] bool cast_tactical_spell(std::int32_t& mana,
                                       FormationMagicState& formation,
                                       const TacticalSpellDef& spell);

struct IndividualSpellDef {
    std::string id;
    SpellScale scale{SpellScale::Local};
    std::int32_t mana_cost{};
    std::int32_t difficulty{};
    std::int32_t modifier{};
    std::int32_t effect_value{};
    std::int32_t failure_backlash{};
};

struct IndividualCasterState {
    std::int32_t mind{};
    std::int32_t mana{};
    std::int32_t health{};
};

struct IndividualCastResult {
    CheckResult check;
    std::int32_t applied_effect{};
    std::int32_t backlash_damage{};
};

[[nodiscard]] IndividualCastResult cast_individual_spell(
    IndividualCasterState& caster, const IndividualSpellDef& spell, std::uint8_t roll,
    const CheckRules& check_rules);

// 無代價且結果可重試時，在 N 次內至少成功一次的機率（萬分比）。
[[nodiscard]] std::uint32_t retry_success_permyriad(std::int32_t mind,
                                                    std::int32_t modifier,
                                                    std::int32_t difficulty,
                                                    std::uint32_t attempts) noexcept;

struct FaithFollower {
    std::uint64_t entity_id{};
    world::Significance base_tier{world::Significance::Ambient};
    world::Significance tier_cap{world::Significance::World};
    Attributes base_attributes;
    std::vector<PowerSourceState> sources;
};

struct RootDeityState {
    const DeityDef* definition{};
    bool alive{true};
};

// Deity 只存在這個 root 容器；Region 狀態刻意不在型別中。
struct RootFaithState {
    std::vector<RootDeityState> deities;
};

struct FaithIntermediaryEvent {
    std::string deity_id;
    std::vector<std::uint64_t> follower_ids;
};

struct DeityFallResult {
    std::uint32_t affected_followers{};
    std::uint32_t total_tiers_lost{};
    FaithIntermediaryEvent event;
};

[[nodiscard]] PowerProfile resolve_follower(const FaithFollower& follower);
[[nodiscard]] std::uint32_t deprive_faith(FaithFollower& follower,
                                          std::string_view deity_id,
                                          std::string reason);
[[nodiscard]] DeityFallResult fall_deity(RootFaithState& root, std::string_view deity_id,
                                         std::span<FaithFollower> followers);
[[nodiscard]] std::uint32_t count_tenet_conflicts(
    std::span<const std::string> selected_tenets, std::span<const TenetDef> definitions);

// 壽命只提供可用年數；能否抵達仍同時受敘事所需時間與種族上限約束。
[[nodiscard]] bool can_reach_within_lifespan(const RaceDef& race,
                                             world::Significance desired_tier,
                                             std::uint32_t required_years) noexcept;

}  // namespace aetheria::rules

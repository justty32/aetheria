#pragma once

// site_combat.h：L2 64x64 戰場上的 cohort 機動、面向、地形與條件式兵種相剋。

#include <array>
#include <cstdint>

#include "core/rules/combat.h"

namespace aetheria::site {

inline constexpr std::int32_t kSiteEngagementStrideSeconds = 900;
inline constexpr std::int32_t kSiteTileMetres = 125;
inline constexpr std::int32_t kFormationSpeedMetresPerHour = 1'500;
inline constexpr std::int32_t kSiteCombatTurns = 16;

enum class CohortRole : std::uint8_t { Spear, Cavalry, Archer, Siege };
enum class CohortFormation : std::uint8_t { Line, SpearWall, Loose };
enum class CohortFacing : std::uint8_t { North, East, South, West };
enum class ContactArc : std::uint8_t { Front, Flank, Rear };

struct CombatGridPosition {
    std::int16_t x{};
    std::int16_t y{};

    constexpr bool operator==(const CombatGridPosition&) const noexcept = default;
};

struct SiteCohort {
    std::uint64_t cohort_id{};
    CohortRole role{CohortRole::Spear};
    CohortFormation formation{CohortFormation::Line};
    CohortFacing facing{CohortFacing::North};
    CombatGridPosition position;
    std::int32_t power{};
    std::int32_t formation_integrity_permyriad{10'000};
    std::int32_t morale_permyriad{10'000};
    std::int32_t moved_tiles_last_turn{};
    bool in_cover{};
    bool behind_wall{};

    constexpr bool operator==(const SiteCohort&) const noexcept = default;
};

struct SiteCombatRules {
    // 加成只有條件成立才套用；1.0 固定為 10000。
    std::array<std::int32_t, 4> conditional_matchup_permyriad{
        15'000, 16'000, 14'000, 18'000};
    std::int32_t intact_formation_threshold_permyriad{8'000};
    std::int32_t archer_minimum_range_tiles{4};
    std::int32_t archer_maximum_range_tiles{8};
    std::int32_t siege_maximum_range_tiles{10};
};

struct SiteAttackResult {
    std::int32_t base_loss{};
    std::int32_t loss_after_command{};
    std::int32_t loss_after_terrain{};
    std::int32_t matchup_multiplier_permyriad{10'000};
    std::int32_t final_loss{};
    std::int32_t distance_tiles{};
    ContactArc contact_arc{ContactArc::Front};
    bool conditional_matchup_applied{};

    constexpr bool operator==(const SiteAttackResult&) const noexcept = default;
};

struct SiteBattleInput {
    rules::CombatSideInput side_a;
    rules::CombatSideInput side_b;
    std::int32_t region_expected_loss_a{};
    std::int32_t region_expected_loss_b{};
    std::int32_t modifier_scale{1'000};
    // M6.7 的 Site 方差幅度在此改作「戰術壓力可挪動的傷亡配額上限」。
    std::int32_t tactical_spread_permyriad{600};
    // 只供故障注入；正常規則固定為 0。
    std::int32_t systematic_bias_permyriad{};
    std::uint64_t event_id{};
    std::uint64_t sample_seed{};
};

struct SiteBattleTelemetry {
    std::array<std::uint64_t, 4> condition_met{};
    std::array<std::uint64_t, 4> condition_not_met{};
    std::int64_t pressure_a_to_b{};
    std::int64_t pressure_b_to_a{};
    std::int32_t maximum_movement_tiles{};
    std::int32_t turns{};
    std::int32_t casualty_shift{};

    constexpr bool operator==(const SiteBattleTelemetry&) const noexcept = default;
};

struct SiteBattleResult {
    std::int32_t loss_a{};
    std::int32_t loss_b{};
    SiteBattleTelemetry telemetry;

    constexpr bool operator==(const SiteBattleResult&) const noexcept = default;
};

[[nodiscard]] constexpr std::int32_t site_engagement_movement_tiles() noexcept {
    return kFormationSpeedMetresPerHour * kSiteEngagementStrideSeconds /
           (3'600 * kSiteTileMetres);
}

[[nodiscard]] std::int32_t combat_distance(CombatGridPosition first,
                                           CombatGridPosition second) noexcept;
[[nodiscard]] ContactArc contact_arc(const SiteCohort& observer,
                                     CombatGridPosition other) noexcept;

[[nodiscard]] SiteAttackResult resolve_site_attack(
    const SiteCohort& attacker, const SiteCohort& defender,
    const rules::CombatModifiers& attacker_modifiers,
    const rules::CombatModifiers& defender_modifiers,
    std::int32_t modifier_scale = 1'000,
    const SiteCombatRules& site_rules = {}) noexcept;

[[nodiscard]] SiteBattleResult simulate_site_battle(
    const SiteBattleInput& input, const SiteCombatRules& site_rules = {});

}  // namespace aetheria::site

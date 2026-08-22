#pragma once

// core/rules/combat.h：Region 一旬戰役的資料驅動整數公式、結構化分解與
// 位階門檻損失配額分配；玩家與 AI 共用同一組純函式。

#include <cstdint>
#include <span>
#include <vector>

#include "core/rules/power.h"

namespace aetheria::rules {

struct CombatExponent {
    std::int32_t numerator{};
    std::int32_t denominator{};
};

struct CombatModifierBounds {
    std::int32_t minimum{};
    std::int32_t maximum{};
};

struct CombatRules {
    std::int32_t modifier_scale{};
    std::int32_t ratio_binary_limit{};
    CombatExponent default_exponent{};
    std::int32_t base_loss_permyriad_per_xun{};
    std::int32_t maximum_duration_xun{};
    CombatModifierBounds terrain{};
    CombatModifierBounds supply{};
    CombatModifierBounds morale{};
    CombatModifierBounds command{};
    CombatModifierBounds posture{};
    std::int32_t collapse_at_min_morale_permyriad{};
    std::int32_t collapse_at_max_morale_permyriad{};
    std::int32_t pursuit_loss_permyriad{};
    std::int32_t supply_attrition_at_min_permyriad{};
    std::int32_t besieging_supply_extra_permyriad{};
    std::int32_t maximum_disease_permyriad{};
    std::int32_t maximum_season_permyriad{};
    std::int32_t desertion_from_supply_permyriad{};
    std::int32_t desertion_from_morale_permyriad{};
    std::int32_t desertion_from_distance_permyriad{};
    std::int32_t morale_loss_rate_divisor{};
    std::int32_t routed_morale_penalty{};
    bool loaded{};
};

struct CombatModifiers {
    std::int32_t terrain{};
    std::int32_t supply{};
    std::int32_t morale{};
    std::int32_t command{};
    std::int32_t posture{};
};

struct CampaignAttrition {
    std::int32_t disease_permyriad{};
    std::int32_t season_permyriad{};
    std::int32_t distance_from_home_permyriad{};
    bool besieging{};
};

struct CombatSideInput {
    std::int32_t power{};
    CombatModifiers modifiers{};
    CampaignAttrition attrition{};
    std::int32_t accumulated_loss_permyriad{};
};

struct CombatInput {
    CombatSideInput side_a{};
    CombatSideInput side_b{};
    CombatExponent exponent{};
    std::int32_t duration_xun{};
};

// R 輸入以 Q28.36 量化；輸出 value = mantissa_q32 / 2^32 * 2^binary_exponent，
// mantissa 固定落在 [1, 2)。
struct FixedPowerFactor {
    std::uint64_t mantissa_q32{};
    std::int32_t binary_exponent{};
};

struct StrengthBreakdown {
    std::int64_t base{};
    std::int64_t after_terrain{};
    std::int64_t after_supply{};
    std::int64_t after_morale{};
    std::int64_t after_command{};
    std::int64_t adjusted{};
};

struct LossBreakdown {
    std::int32_t engagement{};
    std::int32_t supply{};
    std::int32_t disease{};
    std::int32_t desertion{};
    std::int32_t season{};
    std::int32_t pursuit{};
    std::int32_t total{};
};

struct CombatBreakdown {
    StrengthBreakdown strength_a{};
    StrengthBreakdown strength_b{};
    FixedPowerFactor ratio_power{};
    LossBreakdown loss_a{};
    LossBreakdown loss_b{};
    std::int32_t collapse_threshold_a_permyriad{};
    std::int32_t collapse_threshold_b_permyriad{};
};

enum class Outcome : std::uint8_t { Continue, SideARouted, SideBRouted, MutualDisengagement };

// M6.3 只固定欄位與配額扣抵介面；具名者三階段命運判定留給 M6.4。
struct ParticipantFate {
    std::uint64_t participant_id{};
    std::int32_t allocated_loss{};
};

struct CombatResult {
    std::int32_t loss_a{};
    std::int32_t loss_b{};
    Outcome outcome{Outcome::Continue};
    std::int32_t morale_delta_a{};
    std::int32_t morale_delta_b{};
    std::vector<ParticipantFate> named;
    CombatBreakdown breakdown{};
};

struct LossParticipant {
    std::uint64_t participant_id{};
    std::int32_t capacity{};
    world::Significance tier{world::Significance::Ambient};
    bool individual{};
};

[[nodiscard]] FixedPowerFactor fixed_ratio_power(std::int64_t numerator,
                                                  std::int64_t denominator,
                                                  CombatExponent exponent,
                                                  const CombatRules& rules) noexcept;

[[nodiscard]] CombatResult resolve_region_combat(const CombatInput& input,
                                                  const CombatRules& rules) noexcept;

[[nodiscard]] std::vector<ParticipantFate> allocate_combat_loss(
    std::int32_t total_loss, std::span<const LossParticipant> participants,
    world::Significance attacker_tier, const PowerRules& power_rules,
    const PowerBreakthroughDef* breakthrough = nullptr) noexcept;

}  // namespace aetheria::rules

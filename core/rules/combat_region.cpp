// core/rules/combat_region.cpp：Region 戰役的力量修正與雙方結算。

#include "core/rules/combat.h"

#include <limits>

#include "core/base/check.h"
#include "core/rules/combat_detail.h"

namespace aetheria::rules {
namespace {

[[nodiscard]] std::int64_t scaled_step(std::int64_t value, std::int32_t modifier,
                                       std::int32_t scale) noexcept {
    AETH_CHECK(value >= 0);
    AETH_CHECK(modifier > 0);
    AETH_CHECK(scale > 0);
    AETH_CHECK(value <= (std::numeric_limits<std::int64_t>::max() - scale / 2) / modifier);
    return (value * modifier + scale / 2) / scale;
}

void check_modifier(std::int32_t value, const CombatModifierBounds& bounds) noexcept {
    AETH_CHECK(value >= bounds.minimum);
    AETH_CHECK(value <= bounds.maximum);
}

[[nodiscard]] StrengthBreakdown strength_breakdown(const CombatSideInput& side,
                                                    const CombatRules& rules) noexcept {
    AETH_CHECK(side.power > 0);
    check_modifier(side.modifiers.terrain, rules.terrain);
    check_modifier(side.modifiers.supply, rules.supply);
    check_modifier(side.modifiers.morale, rules.morale);
    check_modifier(side.modifiers.command, rules.command);
    check_modifier(side.modifiers.posture, rules.posture);
    StrengthBreakdown result{};
    result.base = side.power;
    result.after_terrain = scaled_step(result.base, side.modifiers.terrain, rules.modifier_scale);
    result.after_supply =
        scaled_step(result.after_terrain, side.modifiers.supply, rules.modifier_scale);
    result.after_morale =
        scaled_step(result.after_supply, side.modifiers.morale, rules.modifier_scale);
    result.after_command =
        scaled_step(result.after_morale, side.modifiers.command, rules.modifier_scale);
    result.adjusted =
        scaled_step(result.after_command, side.modifiers.posture, rules.modifier_scale);
    AETH_CHECK(result.adjusted > 0);
    return result;
}

}  // namespace

using detail::calculate_side_loss;
using detail::morale_delta;

CombatResult resolve_region_combat(const CombatInput& input, const CombatRules& rules) noexcept {
    AETH_CHECK(rules.loaded);
    AETH_CHECK(input.duration_xun > 0);
    AETH_CHECK(input.duration_xun <= rules.maximum_duration_xun);
    CombatResult result{};
    result.breakdown.strength_a = strength_breakdown(input.side_a, rules);
    result.breakdown.strength_b = strength_breakdown(input.side_b, rules);
    result.breakdown.ratio_power =
        fixed_ratio_power(result.breakdown.strength_a.adjusted,
                          result.breakdown.strength_b.adjusted, input.exponent, rules);
    const auto inverse = fixed_ratio_power(result.breakdown.strength_b.adjusted,
                                           result.breakdown.strength_a.adjusted, input.exponent,
                                           rules);
    const auto side_a = calculate_side_loss(input.side_a, inverse, input.duration_xun, rules);
    const auto side_b = calculate_side_loss(input.side_b, result.breakdown.ratio_power,
                                            input.duration_xun, rules);
    result.loss_a = side_a.breakdown.total;
    result.loss_b = side_b.breakdown.total;
    result.breakdown.loss_a = side_a.breakdown;
    result.breakdown.loss_b = side_b.breakdown;
    result.breakdown.collapse_threshold_a_permyriad = side_a.collapse_threshold;
    result.breakdown.collapse_threshold_b_permyriad = side_b.collapse_threshold;
    if (side_a.routed && side_b.routed) {
        result.outcome = Outcome::MutualDisengagement;
    } else if (side_a.routed) {
        result.outcome = Outcome::SideARouted;
    } else if (side_b.routed) {
        result.outcome = Outcome::SideBRouted;
    }
    result.morale_delta_a = morale_delta(input.side_a, side_a, rules);
    result.morale_delta_b = morale_delta(input.side_b, side_b, rules);
    return result;
}

}  // namespace aetheria::rules

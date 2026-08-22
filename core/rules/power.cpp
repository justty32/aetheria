// core/rules/power.cpp：等效戰力的 checked 整數運算與個體位階門檻。

#include "core/rules/power.h"

#include <algorithm>
#include <limits>

#include "core/base/check.h"

namespace aetheria::rules {
namespace {

[[nodiscard]] std::size_t tier_index(world::Significance tier) noexcept {
    const auto index = static_cast<std::size_t>(tier);
    AETH_CHECK(index < kPowerTierCount);
    return index;
}

[[nodiscard]] std::int64_t checked_multiply(std::int64_t left, std::int64_t right) noexcept {
    AETH_CHECK(left >= 0);
    AETH_CHECK(right >= 0);
    AETH_CHECK(right == 0 || left <= std::numeric_limits<std::int64_t>::max() / right);
    return left * right;
}

[[nodiscard]] std::int64_t checked_add(std::int64_t left, std::int64_t right) noexcept {
    AETH_CHECK(left >= 0);
    AETH_CHECK(right >= 0);
    AETH_CHECK(left <= std::numeric_limits<std::int64_t>::max() - right);
    return left + right;
}

}  // namespace

std::int64_t equivalent_power(std::span<const PowerStack> stacks,
                              const PowerRules& rules) noexcept {
    AETH_CHECK(rules.loaded);
    std::int64_t result{};
    for (const auto& stack : stacks) {
        AETH_CHECK(stack.count >= 0);
        AETH_CHECK(stack.quality_percent >= rules.minimum_quality_percent);
        AETH_CHECK(stack.quality_percent <= rules.maximum_quality_percent);
        const auto weighted_count =
            checked_multiply(stack.count, rules.tier_weights[tier_index(stack.tier)]);
        const auto contribution = checked_multiply(weighted_count, stack.quality_percent);
        result = checked_add(result, contribution);
    }
    return result;
}

world::Significance cohort_tier(std::span<const PowerStack> stacks) noexcept {
    auto result = world::Significance::Ambient;
    for (const auto& stack : stacks) {
        AETH_CHECK(stack.count >= 0);
        static_cast<void>(tier_index(stack.tier));
        if (stack.count > 0) {
            result = std::max(result, stack.tier);
        }
    }
    return result;
}

std::int64_t apply_individual_tier_gate(
    std::int64_t proposed_damage, world::Significance attacker_tier,
    world::Significance defender_tier, bool defender_is_individual, const PowerRules& rules,
    const PowerBreakthroughDef* breakthrough) noexcept {
    AETH_CHECK(rules.loaded);
    AETH_CHECK(proposed_damage >= 0);
    const auto attacker = tier_index(attacker_tier);
    const auto defender = tier_index(defender_tier);
    if (!defender_is_individual || breakthrough != nullptr || attacker >= defender ||
        defender - attacker < rules.individual_gate_minimum_gap) {
        return proposed_damage;
    }

    AETH_CHECK(rules.gated_damage_denominator > 0);
    const auto quotient = proposed_damage / rules.gated_damage_denominator;
    const auto remainder = proposed_damage % rules.gated_damage_denominator;
    return checked_add(checked_multiply(quotient, rules.gated_damage_numerator),
                       checked_multiply(remainder, rules.gated_damage_numerator) /
                           rules.gated_damage_denominator);
}

}  // namespace aetheria::rules

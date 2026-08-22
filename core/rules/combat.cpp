// core/rules/combat.cpp：Q28.36 比值 + Q16.16 log2/exp2 的參數化 R^p 與 Region 戰役公式。

#include "core/rules/combat.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <numeric>
#include <vector>

#include "core/base/check.h"

namespace aetheria::rules {
namespace {

constexpr std::uint64_t kQ32One = std::uint64_t{1} << 32U;
constexpr std::uint64_t kQ32Two = std::uint64_t{2} << 32U;
constexpr std::int64_t kQ16One = std::int64_t{1} << 16U;
constexpr std::int32_t kRatioFractionBits = 36;
constexpr std::int32_t kPermyriad = 10'000;

[[nodiscard]] std::uint64_t q32_multiply(std::uint64_t left,
                                         std::uint64_t right) noexcept {
    AETH_CHECK(left < kQ32Two);
    AETH_CHECK(right < kQ32Two);
    const auto left_high = left >> 32U;
    const auto right_high = right >> 32U;
    const auto left_low = left & 0xffff'ffffULL;
    const auto right_low = right & 0xffff'ffffULL;
    return (left_high * right_high << 32U) + left_high * right_low +
           right_high * left_low + ((left_low * right_low) >> 32U);
}

[[nodiscard]] std::uint64_t q32_square_root(std::uint64_t value) noexcept {
    AETH_CHECK(value >= kQ32One);
    AETH_CHECK(value <= kQ32Two);
    auto lower = kQ32One;
    auto upper = kQ32Two - 1U;
    while (lower < upper) {
        const auto middle = lower + (upper - lower + 1U) / 2U;
        if (q32_multiply(middle, middle) <= value) {
            lower = middle;
        } else {
            upper = middle - 1U;
        }
    }
    return lower;
}

[[nodiscard]] bool below_binary_limit(std::uint64_t numerator, std::uint64_t denominator,
                                      std::int32_t limit) noexcept {
    if (denominator > (std::numeric_limits<std::uint64_t>::max() >> limit)) {
        return true;
    }
    return numerator <= (denominator << limit);
}

[[nodiscard]] std::uint64_t ratio_fixed(std::int64_t signed_numerator,
                                        std::int64_t signed_denominator,
                                        const CombatRules& rules) noexcept {
    AETH_CHECK(signed_numerator > 0);
    AETH_CHECK(signed_denominator > 0);
    AETH_CHECK(rules.ratio_binary_limit > 0);
    AETH_CHECK(rules.ratio_binary_limit <= 30);
    const auto numerator = static_cast<std::uint64_t>(signed_numerator);
    const auto denominator = static_cast<std::uint64_t>(signed_denominator);
    const bool ratio_within_domain =
        below_binary_limit(numerator, denominator, rules.ratio_binary_limit) &&
        below_binary_limit(denominator, numerator, rules.ratio_binary_limit);
    AETH_CHECK(ratio_within_domain);

    const auto whole = numerator / denominator;
    auto remainder = numerator % denominator;
    std::uint64_t fraction{};
    for (std::int32_t bit = 0; bit < kRatioFractionBits; ++bit) {
        fraction <<= 1U;
        if (remainder >= denominator - remainder) {
            remainder -= denominator - remainder;
            fraction |= 1U;
        } else {
            remainder *= 2U;
        }
    }
    const auto result = (whole << kRatioFractionBits) | fraction;
    AETH_CHECK(result > 0);
    return result;
}

[[nodiscard]] FixedPowerFactor normalize_ratio(std::uint64_t value) noexcept {
    AETH_CHECK(value > 0);
    const auto highest_bit = static_cast<std::int32_t>(std::bit_width(value) - 1U);
    const auto exponent = highest_bit - kRatioFractionBits;
    const auto mantissa_shift = highest_bit - 32;
    const auto mantissa =
        mantissa_shift >= 0 ? value >> mantissa_shift : value << -mantissa_shift;
    AETH_CHECK(mantissa >= kQ32One);
    AETH_CHECK(mantissa < kQ32Two);
    return {mantissa, exponent};
}

[[nodiscard]] std::int64_t log2_q16(std::uint64_t ratio) noexcept {
    auto normalized = normalize_ratio(ratio);
    std::int64_t result = static_cast<std::int64_t>(normalized.binary_exponent) * kQ16One;
    auto value = normalized.mantissa_q32;
    for (std::int32_t bit = 15; bit >= 0; --bit) {
        value = q32_multiply(value, value);
        if (value >= kQ32Two) {
            value >>= 1U;
            result |= std::int64_t{1} << bit;
        }
    }
    return result;
}

[[nodiscard]] std::int64_t divide_nearest(std::int64_t numerator,
                                          std::int32_t denominator) noexcept {
    AETH_CHECK(denominator > 0);
    if (numerator >= 0) {
        return (numerator + denominator / 2) / denominator;
    }
    return -((-numerator + denominator / 2) / denominator);
}

[[nodiscard]] FixedPowerFactor exp2_q16(std::int64_t exponent_q16) noexcept {
    auto integral = exponent_q16 / kQ16One;
    auto fractional = exponent_q16 % kQ16One;
    if (fractional < 0) {
        fractional += kQ16One;
        --integral;
    }

    static const auto roots = [] {
        std::array<std::uint64_t, 16> result{};
        result[0] = q32_square_root(kQ32Two);
        for (std::size_t index = 1; index < result.size(); ++index) {
            result[index] = q32_square_root(result[index - 1]);
        }
        return result;
    }();

    auto mantissa = kQ32One;
    for (std::size_t index = 0; index < roots.size(); ++index) {
        const auto bit = 15U - index;
        if ((static_cast<std::uint64_t>(fractional) & (std::uint64_t{1} << bit)) != 0U) {
            mantissa = q32_multiply(mantissa, roots[index]);
        }
    }
    AETH_CHECK(integral >= std::numeric_limits<std::int32_t>::min());
    AETH_CHECK(integral <= std::numeric_limits<std::int32_t>::max());
    return {mantissa, static_cast<std::int32_t>(integral)};
}

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

[[nodiscard]] std::int32_t factor_rate(std::int64_t rate, FixedPowerFactor factor) noexcept {
    AETH_CHECK(rate >= 0);
    AETH_CHECK(static_cast<std::uint64_t>(rate) <=
               std::numeric_limits<std::uint64_t>::max() / factor.mantissa_q32);
    auto scaled = static_cast<std::uint64_t>(rate) * factor.mantissa_q32;
    if (factor.binary_exponent >= 0) {
        for (std::int32_t step = 0; step < factor.binary_exponent; ++step) {
            if (scaled >= (static_cast<std::uint64_t>(kPermyriad) << 31U)) {
                return kPermyriad;
            }
            scaled *= 2U;
        }
    } else {
        for (std::int32_t step = 0; step < -factor.binary_exponent; ++step) {
            scaled >>= 1U;
        }
    }
    const auto rounded = (scaled + (std::uint64_t{1} << 31U)) >> 32U;
    return static_cast<std::int32_t>(std::min<std::uint64_t>(rounded, kPermyriad));
}

[[nodiscard]] std::int32_t loss_for_rate(std::int32_t power, std::int64_t rate) noexcept {
    AETH_CHECK(power > 0);
    AETH_CHECK(rate >= 0);
    const auto capped = std::min<std::int64_t>(rate, kPermyriad);
    return static_cast<std::int32_t>((static_cast<std::int64_t>(power) * capped +
                                      kPermyriad / 2) /
                                     kPermyriad);
}

[[nodiscard]] std::int32_t interpolate(std::int32_t input, CombatModifierBounds bounds,
                                       std::int32_t output_at_min,
                                       std::int32_t output_at_max) noexcept {
    AETH_CHECK(input >= bounds.minimum);
    AETH_CHECK(input <= bounds.maximum);
    AETH_CHECK(bounds.minimum < bounds.maximum);
    const auto position = static_cast<std::int64_t>(input - bounds.minimum);
    const auto range = static_cast<std::int64_t>(bounds.maximum - bounds.minimum);
    return static_cast<std::int32_t>(
        output_at_min + position * (output_at_max - output_at_min) / range);
}

[[nodiscard]] std::int32_t deficit_permyriad(std::int32_t value,
                                             CombatModifierBounds bounds) noexcept {
    return interpolate(value, bounds, kPermyriad, 0);
}

[[nodiscard]] std::int32_t rate_component(std::int32_t coefficient,
                                          std::int32_t exposure) noexcept {
    return static_cast<std::int32_t>(
        (static_cast<std::int64_t>(coefficient) * exposure + kPermyriad / 2) / kPermyriad);
}

void trim_to_total(LossBreakdown& loss, std::int32_t allowed) noexcept {
    AETH_CHECK(allowed >= 0);
    std::int32_t remaining = allowed;
    for (auto* component : {&loss.supply, &loss.disease, &loss.desertion, &loss.season}) {
        *component = std::min(*component, remaining);
        remaining -= *component;
    }
    loss.engagement = std::min(loss.engagement, remaining);
    loss.pursuit = 0;
    loss.total = allowed - (remaining - loss.engagement);
}

struct SideLoss {
    LossBreakdown breakdown{};
    std::int32_t collapse_threshold{};
    bool routed{};
};

[[nodiscard]] SideLoss calculate_side_loss(const CombatSideInput& side, FixedPowerFactor factor,
                                            std::int32_t duration,
                                            const CombatRules& rules) noexcept {
    AETH_CHECK(side.accumulated_loss_permyriad >= 0);
    AETH_CHECK(side.accumulated_loss_permyriad <= kPermyriad);
    AETH_CHECK(side.attrition.disease_permyriad >= 0);
    AETH_CHECK(side.attrition.disease_permyriad <= rules.maximum_disease_permyriad);
    AETH_CHECK(side.attrition.season_permyriad >= 0);
    AETH_CHECK(side.attrition.season_permyriad <= rules.maximum_season_permyriad);
    AETH_CHECK(side.attrition.distance_from_home_permyriad >= 0);
    AETH_CHECK(side.attrition.distance_from_home_permyriad <= kPermyriad);

    SideLoss result{};
    const auto engagement_rate = factor_rate(
        static_cast<std::int64_t>(rules.base_loss_permyriad_per_xun) * duration, factor);
    result.breakdown.engagement = loss_for_rate(side.power, engagement_rate);

    auto supply_rate = interpolate(side.modifiers.supply, rules.supply,
                                   rules.supply_attrition_at_min_permyriad, 0);
    if (side.attrition.besieging) {
        supply_rate += rules.besieging_supply_extra_permyriad;
    }
    result.breakdown.supply = loss_for_rate(side.power, supply_rate * duration);
    result.breakdown.disease =
        loss_for_rate(side.power, side.attrition.disease_permyriad * duration);
    result.breakdown.season =
        loss_for_rate(side.power, side.attrition.season_permyriad * duration);
    const auto desertion_rate =
        rate_component(rules.desertion_from_supply_permyriad,
                       deficit_permyriad(side.modifiers.supply, rules.supply)) +
        rate_component(rules.desertion_from_morale_permyriad,
                       deficit_permyriad(side.modifiers.morale, rules.morale)) +
        rate_component(rules.desertion_from_distance_permyriad,
                       side.attrition.distance_from_home_permyriad);
    result.breakdown.desertion = loss_for_rate(side.power, desertion_rate * duration);

    const auto raw_total = static_cast<std::int64_t>(result.breakdown.engagement) +
                           result.breakdown.supply + result.breakdown.disease +
                           result.breakdown.desertion + result.breakdown.season;
    result.breakdown.total =
        static_cast<std::int32_t>(std::min<std::int64_t>(raw_total, side.power));
    result.collapse_threshold =
        interpolate(side.modifiers.morale, rules.morale,
                    rules.collapse_at_min_morale_permyriad,
                    rules.collapse_at_max_morale_permyriad);
    const auto projected_rate = static_cast<std::int32_t>(
        (static_cast<std::int64_t>(result.breakdown.total) * kPermyriad + side.power - 1) /
        side.power);
    result.routed =
        side.accumulated_loss_permyriad + projected_rate > result.collapse_threshold;
    if (result.routed) {
        const auto allowed_rate =
            std::max(0, result.collapse_threshold - side.accumulated_loss_permyriad + 1);
        const auto allowed_loss = static_cast<std::int32_t>(
            (static_cast<std::int64_t>(side.power) * allowed_rate + kPermyriad - 1) /
            kPermyriad);
        if (result.breakdown.total > allowed_loss) {
            trim_to_total(result.breakdown, allowed_loss);
        }
        const auto remaining = side.power - result.breakdown.total;
        result.breakdown.pursuit = loss_for_rate(remaining, rules.pursuit_loss_permyriad);
        result.breakdown.total += result.breakdown.pursuit;
    }
    AETH_CHECK(result.breakdown.total <= side.power);
    return result;
}

[[nodiscard]] std::int32_t morale_delta(const CombatSideInput& side, const SideLoss& loss,
                                        const CombatRules& rules) noexcept {
    const auto loss_rate = static_cast<std::int32_t>(
        (static_cast<std::int64_t>(loss.breakdown.total) * kPermyriad + side.power - 1) /
        side.power);
    return -(loss_rate / rules.morale_loss_rate_divisor) -
           (loss.routed ? rules.routed_morale_penalty : 0);
}

}  // namespace

FixedPowerFactor fixed_ratio_power(std::int64_t numerator, std::int64_t denominator,
                                    CombatExponent exponent,
                                    const CombatRules& rules) noexcept {
    AETH_CHECK(rules.loaded);
    AETH_CHECK(exponent.numerator > 0);
    AETH_CHECK(exponent.denominator > 0);
    AETH_CHECK(static_cast<std::int64_t>(exponent.numerator) <=
               static_cast<std::int64_t>(exponent.denominator) * 4);
    const auto ratio = ratio_fixed(numerator, denominator, rules);
    const auto normalized = normalize_ratio(ratio);
    if (exponent.numerator == exponent.denominator) {
        return normalized;
    }
    if (static_cast<std::int64_t>(exponent.numerator) ==
        static_cast<std::int64_t>(exponent.denominator) * 2) {
        auto mantissa = q32_multiply(normalized.mantissa_q32, normalized.mantissa_q32);
        auto binary_exponent = normalized.binary_exponent * 2;
        if (mantissa >= kQ32Two) {
            mantissa >>= 1U;
            ++binary_exponent;
        }
        return {mantissa, binary_exponent};
    }
    const auto scaled_log = divide_nearest(log2_q16(ratio) * exponent.numerator,
                                           exponent.denominator);
    return exp2_q16(scaled_log);
}

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

std::vector<ParticipantFate> allocate_combat_loss(
    std::int32_t total_loss, std::span<const LossParticipant> participants,
    world::Significance attacker_tier, const PowerRules& power_rules,
    const PowerBreakthroughDef* breakthrough) noexcept {
    AETH_CHECK(total_loss >= 0);
    std::vector<ParticipantFate> result;
    result.reserve(participants.size());
    std::vector<std::int32_t> weights;
    weights.reserve(participants.size());
    std::int64_t total_weight{};
    for (const auto& participant : participants) {
        AETH_CHECK(participant.capacity >= 0);
        const auto weight = static_cast<std::int32_t>(apply_individual_tier_gate(
            participant.capacity, attacker_tier, participant.tier, participant.individual,
            power_rules, breakthrough));
        weights.push_back(weight);
        total_weight += weight;
        result.push_back({participant.participant_id, 0});
    }
    AETH_CHECK(total_loss == 0 || total_weight >= total_loss);
    if (total_loss == 0) {
        return result;
    }

    struct Remainder {
        std::int64_t value{};
        std::size_t index{};
    };
    std::vector<Remainder> remainders;
    remainders.reserve(participants.size());
    std::int32_t allocated{};
    for (std::size_t index = 0; index < participants.size(); ++index) {
        const auto product = static_cast<std::int64_t>(total_loss) * weights[index];
        result[index].allocated_loss = static_cast<std::int32_t>(product / total_weight);
        allocated += result[index].allocated_loss;
        remainders.push_back({product % total_weight, index});
    }
    std::ranges::sort(remainders, [](const Remainder& left, const Remainder& right) {
        if (left.value != right.value) {
            return left.value > right.value;
        }
        return left.index < right.index;
    });
    const auto missing = total_loss - allocated;
    AETH_CHECK(missing >= 0);
    AETH_CHECK(static_cast<std::size_t>(missing) <= remainders.size());
    for (std::int32_t index = 0; index < missing; ++index) {
        ++result[remainders[static_cast<std::size_t>(index)].index].allocated_loss;
    }
    AETH_CHECK(std::accumulate(result.begin(), result.end(), std::int64_t{},
                               [](std::int64_t sum, const ParticipantFate& fate) {
                                   return sum + fate.allocated_loss;
                               }) == total_loss);
    return result;
}

}  // namespace aetheria::rules

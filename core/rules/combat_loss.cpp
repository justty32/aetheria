// core/rules/combat_loss.cpp：單方戰損、潰散追擊與士氣變化。

#include "core/rules/combat_detail.h"

#include <algorithm>
#include <limits>

#include "core/base/check.h"

namespace aetheria::rules {
namespace {

constexpr std::int32_t kPermyriad = 10'000;

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

}  // namespace

namespace detail {

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

}  // namespace detail

}  // namespace aetheria::rules

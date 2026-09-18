// site_combat.cpp：以 15 分鐘 stride 跑 16 回合 cohort 戰場，戰術只挪動 Region 傷亡配額。

#include "core/site/site_combat_detail.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "core/base/check.h"
#include "core/worldgen/region_seed.h"

namespace aetheria::site {
using combat_detail::desired_range;
using combat_detail::deploy_side;
using combat_detail::kPermyriad;
using combat_detail::move_toward;
using combat_detail::role_index;
using combat_detail::scaled_nearest;

namespace {
constexpr std::int32_t kTacticalPressureAmplification = 4;
constexpr std::uint64_t kSideASalt = UINT64_C(0x43a7529db694c821);
constexpr std::uint64_t kSideBSalt = UINT64_C(0xb95d18f24c73a60e);

void degrade_after_hit(SiteCohort& cohort, std::int32_t loss) noexcept {
    const auto power_before_hit = cohort.power;
    const auto rate = static_cast<std::int32_t>(
        std::min<std::int64_t>(1'000, static_cast<std::int64_t>(loss) * kPermyriad /
                                         std::max(1, power_before_hit)));
    // Region 配額裁決最終總量；此處的逐 cohort 損傷仍會即時削弱後續回合輸出。
    cohort.power = std::max(1, power_before_hit - loss);
    cohort.formation_integrity_permyriad =
        std::max(0, cohort.formation_integrity_permyriad - rate);
    cohort.morale_permyriad = std::max(0, cohort.morale_permyriad - rate / 2);
}

[[nodiscard]] std::int32_t tactical_shift(const SiteBattleInput& input,
                                          std::int64_t pressure_a,
                                          std::int64_t pressure_b) noexcept {
    if (pressure_a == 0 && pressure_b == 0) {
        return 0;
    }
    const auto efficiency_a = pressure_a * kPermyriad / input.side_a.power;
    const auto efficiency_b = pressure_b * kPermyriad / input.side_b.power;
    const auto denominator = std::max<std::int64_t>(1, std::abs(efficiency_a) +
                                                          std::abs(efficiency_b));
    const auto balance = std::clamp<std::int64_t>(
        (efficiency_a - efficiency_b) * kPermyriad * kTacticalPressureAmplification /
            denominator,
        -kPermyriad, kPermyriad);
    const auto quota = std::min(input.region_expected_loss_a,
                                input.region_expected_loss_b);
    return static_cast<std::int32_t>(static_cast<std::int64_t>(quota) * balance *
                                     input.tactical_spread_permyriad /
                                     (static_cast<std::int64_t>(kPermyriad) * kPermyriad));
}

[[nodiscard]] std::int32_t apply_bias(std::int32_t loss, std::int32_t capacity,
                                      std::int32_t bias_permyriad) noexcept {
    const auto multiplier = kPermyriad + bias_permyriad;
    AETH_CHECK(multiplier >= 0);
    return std::min(capacity, scaled_nearest(loss, multiplier, kPermyriad));
}

}  // namespace

SiteBattleResult simulate_site_battle(const SiteBattleInput& input,
                                      const SiteCombatRules& site_rules) {
    AETH_CHECK(input.side_a.power > 0);
    AETH_CHECK(input.side_b.power > 0);
    AETH_CHECK(input.region_expected_loss_a >= 0);
    AETH_CHECK(input.region_expected_loss_b >= 0);
    AETH_CHECK(input.region_expected_loss_a <= input.side_a.power);
    AETH_CHECK(input.region_expected_loss_b <= input.side_b.power);
    AETH_CHECK(input.modifier_scale > 0);
    AETH_CHECK(input.tactical_spread_permyriad >= 0);
    AETH_CHECK(input.tactical_spread_permyriad <= kPermyriad);
    AETH_CHECK(input.systematic_bias_permyriad >= -kPermyriad);

    const auto seed = worldgen::splitmix64(input.event_id ^ input.sample_seed);
    auto side_a = deploy_side(input.side_a.power, 0, seed ^ kSideASalt);
    auto side_b = deploy_side(input.side_b.power, 1, seed ^ kSideBSalt);
    SiteBattleResult result;
    result.telemetry.turns = kSiteCombatTurns;
    for (std::int32_t turn = 0; turn < kSiteCombatTurns; ++turn) {
        for (std::size_t index = 0; index < side_a.size(); ++index) {
            const auto target = index % side_b.size();
            const auto draw = worldgen::splitmix64(seed ^
                                                   (static_cast<std::uint64_t>(turn) << 32U) ^
                                                   index);
            result.telemetry.maximum_movement_tiles = std::max(
                result.telemetry.maximum_movement_tiles,
                move_toward(side_a[index], side_b[target], draw));
        }
        for (std::size_t index = 0; index < side_b.size(); ++index) {
            const auto target = index % side_a.size();
            const auto draw = worldgen::splitmix64(seed ^ kSideBSalt ^
                                                   (static_cast<std::uint64_t>(turn) << 32U) ^
                                                   index);
            result.telemetry.maximum_movement_tiles = std::max(
                result.telemetry.maximum_movement_tiles,
                move_toward(side_b[index], side_a[target], draw));
        }
        for (std::size_t index = 0; index < side_a.size(); ++index) {
            auto& attacker = side_a[index];
            auto& defender = side_b[index % side_b.size()];
            const auto attack = resolve_site_attack(attacker, defender,
                                                    input.side_a.modifiers,
                                                    input.side_b.modifiers,
                                                    input.modifier_scale, site_rules);
            const auto in_range = attack.distance_tiles <= desired_range(attacker.role);
            if (in_range) {
                result.telemetry.pressure_a_to_b += attack.final_loss;
                const auto role = role_index(attacker.role);
                ++(attack.conditional_matchup_applied
                       ? result.telemetry.condition_met[role]
                       : result.telemetry.condition_not_met[role]);
                degrade_after_hit(defender, attack.final_loss);
            }
        }
        for (std::size_t index = 0; index < side_b.size(); ++index) {
            auto& attacker = side_b[index];
            auto& defender = side_a[index % side_a.size()];
            const auto attack = resolve_site_attack(attacker, defender,
                                                    input.side_b.modifiers,
                                                    input.side_a.modifiers,
                                                    input.modifier_scale, site_rules);
            const auto in_range = attack.distance_tiles <= desired_range(attacker.role);
            if (in_range) {
                result.telemetry.pressure_b_to_a += attack.final_loss;
                const auto role = role_index(attacker.role);
                ++(attack.conditional_matchup_applied
                       ? result.telemetry.condition_met[role]
                       : result.telemetry.condition_not_met[role]);
                degrade_after_hit(defender, attack.final_loss);
            }
        }
    }

    auto shift = tactical_shift(input, result.telemetry.pressure_a_to_b,
                                result.telemetry.pressure_b_to_a);
    const auto minimum_shift = std::max(input.region_expected_loss_a - input.side_a.power,
                                        -input.region_expected_loss_b);
    const auto maximum_shift = std::min(input.region_expected_loss_a,
                                        input.side_b.power - input.region_expected_loss_b);
    shift = std::clamp(shift, minimum_shift, maximum_shift);
    result.telemetry.casualty_shift = shift;
    const auto tactical_loss_a = input.region_expected_loss_a - shift;
    const auto tactical_loss_b = input.region_expected_loss_b + shift;
    result.loss_a = apply_bias(tactical_loss_a, input.side_a.power,
                               input.systematic_bias_permyriad);
    result.loss_b = apply_bias(tactical_loss_b, input.side_b.power,
                               input.systematic_bias_permyriad);
    return result;
}

}  // namespace aetheria::site

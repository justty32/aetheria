// site_combat.cpp：以 15 分鐘 stride 跑 16 回合 cohort 戰場，戰術只挪動 Region 傷亡配額。

#include "core/site/site_combat.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "core/base/check.h"
#include "core/worldgen/region_seed.h"

namespace aetheria::site {
namespace {

constexpr std::int32_t kPermyriad = 10'000;
constexpr std::int32_t kTacticalPressureAmplification = 4;
constexpr std::size_t kMaximumCohortsPerSide = 8;
constexpr std::uint64_t kSideASalt = UINT64_C(0x43a7529db694c821);
constexpr std::uint64_t kSideBSalt = UINT64_C(0xb95d18f24c73a60e);

[[nodiscard]] constexpr std::size_t role_index(CohortRole role) noexcept {
    return static_cast<std::size_t>(role);
}

[[nodiscard]] constexpr std::array<std::int32_t, 2>
facing_vector(CohortFacing facing) noexcept {
    switch (facing) {
        case CohortFacing::North:
            return {0, -1};
        case CohortFacing::East:
            return {1, 0};
        case CohortFacing::South:
            return {0, 1};
        case CohortFacing::West:
            return {-1, 0};
    }
    return {0, 0};
}

[[nodiscard]] std::int32_t scaled_nearest(std::int32_t value,
                                          std::int32_t multiplier,
                                          std::int32_t scale) noexcept {
    AETH_CHECK(value >= 0);
    AETH_CHECK(multiplier > 0);
    AETH_CHECK(scale > 0);
    const auto numerator = static_cast<std::int64_t>(value) * multiplier + scale / 2;
    return static_cast<std::int32_t>(std::min<std::int64_t>(
        numerator / scale, std::numeric_limits<std::int32_t>::max()));
}

[[nodiscard]] std::int32_t divided_nearest(std::int32_t value,
                                           std::int32_t scale,
                                           std::int32_t divisor) noexcept {
    AETH_CHECK(value >= 0);
    AETH_CHECK(scale > 0);
    AETH_CHECK(divisor > 0);
    const auto numerator = static_cast<std::int64_t>(value) * scale + divisor / 2;
    return static_cast<std::int32_t>(std::min<std::int64_t>(
        numerator / divisor, std::numeric_limits<std::int32_t>::max()));
}

[[nodiscard]] bool condition_applies(const SiteCohort& attacker,
                                     const SiteCohort& defender,
                                     std::int32_t distance,
                                     const SiteCombatRules& rules) noexcept {
    switch (attacker.role) {
        case CohortRole::Spear:
            return defender.role == CohortRole::Cavalry && distance == 1 &&
                   attacker.formation == CohortFormation::SpearWall &&
                   attacker.formation_integrity_permyriad >=
                       rules.intact_formation_threshold_permyriad &&
                   contact_arc(attacker, defender.position) == ContactArc::Front;
        case CohortRole::Cavalry:
            return defender.role == CohortRole::Archer && distance == 1 &&
                   contact_arc(defender, attacker.position) != ContactArc::Front;
        case CohortRole::Archer:
            return defender.role == CohortRole::Spear &&
                   distance >= rules.archer_minimum_range_tiles &&
                   distance <= rules.archer_maximum_range_tiles;
        case CohortRole::Siege:
            return defender.behind_wall && attacker.moved_tiles_last_turn == 0 &&
                   attacker.in_cover && distance <= rules.siege_maximum_range_tiles;
    }
    return false;
}

[[nodiscard]] std::size_t cohort_count(std::int32_t power) noexcept {
    AETH_CHECK(power > 0);
    return std::min<std::size_t>(kMaximumCohortsPerSide,
                                 static_cast<std::size_t>((power + 9'999) / 10'000));
}

[[nodiscard]] CohortFormation default_formation(CohortRole role) noexcept {
    if (role == CohortRole::Spear) {
        return CohortFormation::SpearWall;
    }
    if (role == CohortRole::Archer) {
        return CohortFormation::Loose;
    }
    return CohortFormation::Line;
}

[[nodiscard]] std::vector<SiteCohort> deploy_side(std::int32_t power,
                                                  std::uint8_t side,
                                                  std::uint64_t seed) {
    const auto count = cohort_count(power);
    std::vector<SiteCohort> result;
    result.reserve(count);
    const auto base_power = power / static_cast<std::int32_t>(count);
    const auto remainder = power % static_cast<std::int32_t>(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto draw = worldgen::splitmix64(seed ^ index);
        const auto role = static_cast<CohortRole>((index + (draw >> 8U) % 4U) % 4U);
        const auto y = static_cast<std::int16_t>(
            5 + (index * 53U / std::max<std::size_t>(1, count - 1U)));
        result.push_back({
            .cohort_id = (static_cast<std::uint64_t>(side) << 63U) |
                         (static_cast<std::uint64_t>(index) + 1U),
            .role = role,
            .formation = default_formation(role),
            .facing = side == 0U ? CohortFacing::East : CohortFacing::West,
            .position = {static_cast<std::int16_t>(side == 0U ? 4 : 59), y},
            .power = base_power +
                     (static_cast<std::int32_t>(index) < remainder ? 1 : 0),
            .formation_integrity_permyriad =
                static_cast<std::int32_t>(7'000 + (draw >> 16U) % 3'001U),
            .morale_permyriad =
                static_cast<std::int32_t>(8'000 + (draw >> 32U) % 2'001U),
            .in_cover = ((draw >> 48U) & 1U) != 0U,
            .behind_wall = ((draw >> 49U) & 3U) == 0U,
        });
    }
    return result;
}

[[nodiscard]] std::int32_t step_axis(std::int16_t& coordinate,
                                     std::int16_t target,
                                     std::int32_t budget) noexcept {
    const auto difference = static_cast<std::int32_t>(target) - coordinate;
    const auto step = std::clamp(difference, -budget, budget);
    coordinate = static_cast<std::int16_t>(coordinate + step);
    return std::abs(step);
}

[[nodiscard]] std::int32_t desired_range(CohortRole role) noexcept {
    switch (role) {
        case CohortRole::Archer:
            return 6;
        case CohortRole::Siege:
            return 9;
        case CohortRole::Spear:
        case CohortRole::Cavalry:
            return 1;
    }
    return 1;
}

[[nodiscard]] std::int32_t move_toward(SiteCohort& cohort,
                                       const SiteCohort& target,
                                       std::uint64_t draw) noexcept {
    const auto distance = combat_distance(cohort.position, target.position);
    if (distance <= desired_range(cohort.role)) {
        cohort.moved_tiles_last_turn = 0;
        return 0;
    }
    auto budget = site_engagement_movement_tiles();
    std::int32_t moved{};
    if (cohort.role == CohortRole::Cavalry && distance <= 7) {
        const auto flank_y = static_cast<std::int16_t>(
            std::clamp<std::int32_t>(target.position.y + (((draw >> 8U) & 1U) != 0U ? 1 : -1),
                                     0, 63));
        const auto step = step_axis(cohort.position.y, flank_y, budget);
        moved += step;
        budget -= step;
    }
    const auto x_target = static_cast<std::int16_t>(
        target.position.x + (cohort.position.x < target.position.x ? -desired_range(cohort.role)
                                                                    : desired_range(cohort.role)));
    const auto x_step = step_axis(cohort.position.x, x_target, budget);
    moved += x_step;
    budget -= x_step;
    moved += step_axis(cohort.position.y, target.position.y, budget);
    cohort.moved_tiles_last_turn = moved;
    return moved;
}

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

std::int32_t combat_distance(CombatGridPosition first,
                             CombatGridPosition second) noexcept {
    return std::abs(static_cast<std::int32_t>(first.x) - second.x) +
           std::abs(static_cast<std::int32_t>(first.y) - second.y);
}

ContactArc contact_arc(const SiteCohort& observer,
                       CombatGridPosition other) noexcept {
    const auto facing = facing_vector(observer.facing);
    const auto dx = static_cast<std::int32_t>(other.x) - observer.position.x;
    const auto dy = static_cast<std::int32_t>(other.y) - observer.position.y;
    const auto forward = dx * facing[0] + dy * facing[1];
    const auto lateral = dx * -facing[1] + dy * facing[0];
    if (std::abs(forward) >= std::abs(lateral)) {
        return forward >= 0 ? ContactArc::Front : ContactArc::Rear;
    }
    return ContactArc::Flank;
}

SiteAttackResult resolve_site_attack(
    const SiteCohort& attacker, const SiteCohort& defender,
    const rules::CombatModifiers& attacker_modifiers,
    const rules::CombatModifiers& defender_modifiers,
    std::int32_t modifier_scale,
    const SiteCombatRules& site_rules) noexcept {
    AETH_CHECK(attacker.power > 0);
    AETH_CHECK(defender.power > 0);
    AETH_CHECK(modifier_scale > 0);
    AETH_CHECK(attacker_modifiers.command > 0);
    AETH_CHECK(defender_modifiers.terrain > 0);
    SiteAttackResult result;
    result.distance_tiles = combat_distance(attacker.position, defender.position);
    result.contact_arc = contact_arc(defender, attacker.position);
    result.base_loss = std::max(1, attacker.power / 100);
    result.loss_after_command = scaled_nearest(result.base_loss,
                                               attacker_modifiers.command,
                                               modifier_scale);
    result.loss_after_terrain = divided_nearest(result.loss_after_command,
                                                modifier_scale,
                                                defender_modifiers.terrain);
    result.conditional_matchup_applied =
        condition_applies(attacker, defender, result.distance_tiles, site_rules);
    if (result.conditional_matchup_applied) {
        result.matchup_multiplier_permyriad =
            site_rules.conditional_matchup_permyriad[role_index(attacker.role)];
    }
    result.final_loss = scaled_nearest(result.loss_after_terrain,
                                       result.matchup_multiplier_permyriad,
                                       kPermyriad);
    return result;
}

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

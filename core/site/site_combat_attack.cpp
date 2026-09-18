// site_combat_attack.cpp：面向、距離與單次 cohort 攻擊損傷。

#include "core/site/site_combat_detail.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "core/base/check.h"

namespace aetheria::site::combat_detail {

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

}  // namespace aetheria::site::combat_detail

namespace aetheria::site {
using combat_detail::kPermyriad;
using combat_detail::role_index;
using combat_detail::scaled_nearest;

namespace {

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

}  // namespace aetheria::site

// site_combat_cohorts.cpp：cohort 初始部署與逐回合機動。

#include "core/site/site_combat_detail.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/base/check.h"
#include "core/worldgen/region_seed.h"

namespace aetheria::site::combat_detail {
namespace {

constexpr std::size_t kMaximumCohortsPerSide = 8;

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

[[nodiscard]] std::int32_t step_axis(std::int16_t& coordinate,
                                     std::int16_t target,
                                     std::int32_t budget) noexcept {
    const auto difference = static_cast<std::int32_t>(target) - coordinate;
    const auto step = std::clamp(difference, -budget, budget);
    coordinate = static_cast<std::int16_t>(coordinate + step);
    return std::abs(step);
}

}  // namespace

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

}  // namespace aetheria::site::combat_detail

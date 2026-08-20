#include "core/worldgen/city_selection.h"

#include "core/worldgen/civ_tiles.h"
#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace aetheria::worldgen::detail {
namespace {

[[nodiscard]] std::uint32_t manhattan(world::RegionXY lhs, world::RegionXY rhs) noexcept {
    return static_cast<std::uint32_t>(std::abs(static_cast<int>(lhs.x) - static_cast<int>(rhs.x)) +
                                      std::abs(static_cast<int>(lhs.y) - static_cast<int>(rhs.y)));
}

}  // namespace

std::vector<CitySite>
select_city_sites(const QuantizedElevation& elevation, const CityStageOutput& scored,
                  std::uint64_t stage_seed, const SettlementSelectionParameters& parameters) {
    const auto count = elevation.meters.size();
    if (scored.width != elevation.width || scored.height != elevation.height ||
        scored.score.size() != count || scored.bottleneck.size() != count ||
        elevation.land.size() != count ||
        parameters.city_count + parameters.town_count > parameters.target_count) {
        throw std::invalid_argument{"聚落選址參數或評分尺寸不一致"};
    }
    std::vector<std::size_t> candidates;
    candidates.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        if (elevation.land[index] != 0 && scored.score[index] >= parameters.minimum_score) {
            candidates.push_back(index);
        }
    }
    std::ranges::sort(candidates, [&](std::size_t lhs, std::size_t rhs) {
        if (scored.score[lhs] != scored.score[rhs]) {
            return scored.score[lhs] > scored.score[rhs];
        }
        const auto lhs_priority = splitmix64(stage_seed ^ lhs);
        const auto rhs_priority = splitmix64(stage_seed ^ rhs);
        return lhs_priority != rhs_priority ? lhs_priority > rhs_priority : lhs < rhs;
    });

    std::vector<CitySite> selected;
    selected.reserve(parameters.target_count);
    for (const auto index : candidates) {
        if (selected.size() >= parameters.target_count) {
            break;
        }
        const auto accepted = selected.size();
        const auto tier = accepted < parameters.city_count ? world::SettlementTier::City
                          : accepted < parameters.city_count + parameters.town_count
                              ? world::SettlementTier::Town
                              : world::SettlementTier::Village;
        const auto spacing = parameters.minimum_spacing[static_cast<std::size_t>(tier) - 1U];
        const auto tile = coordinate(index, elevation.width);
        const bool too_close = std::ranges::any_of(selected, [&](const CitySite& site) {
            return manhattan(tile, site.tile) < std::max(spacing, site.minimum_spacing);
        });
        if (!too_close) {
            selected.push_back(
                {static_cast<std::uint32_t>(index), tile, scored.score[index], tier, spacing});
        }
    }
    return selected;
}

}  // namespace aetheria::worldgen::detail

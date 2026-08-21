#pragma once

#include "core/world/region_tiles.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
#include <cstdint>

namespace aetheria::tests {

inline constexpr std::uint64_t kReductionWorldSeed = UINT64_C(0xA37E1222);
inline constexpr std::uint32_t kReductionRegionId = 7;
inline constexpr world::RegionXY kReductionCoordinate{0, 0};

[[nodiscard]] inline world::RegionTiles reduction_region(
    world::SettlementTier settlement = world::SettlementTier::Town) {
    const auto& ruleset = test_ruleset();
    world::RegionTiles tiles{1, 1};
    tiles.base[0] = *ruleset.find_terrain("terrain.grassland");
    tiles.relief[0] = *ruleset.find_relief("relief.plain");
    tiles.feature[0] = *ruleset.find_feature("feature.none");
    std::ranges::fill(tiles.edges, *ruleset.find_edge("edge.none"));
    tiles.settlement[0] = settlement;
    return tiles;
}

}  // namespace aetheria::tests

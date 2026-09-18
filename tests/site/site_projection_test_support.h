#pragma once

#include "core/site/site_projection.h"
#include "tests/support/ruleset_fixture.h"

namespace aetheria::tests::site_projection_support {

using aetheria::world::RegionTiles;

[[nodiscard]] inline RegionTiles sample_region_tile() {
    const auto& ruleset = test_ruleset();
    RegionTiles tiles{1, 1};
    tiles.base[0] = *ruleset.find_terrain("terrain.grassland");
    tiles.relief[0] = *ruleset.find_relief("relief.plain");
    tiles.feature[0] = *ruleset.find_feature("feature.none");
    tiles.elevation[0] = 3200;
    tiles.edges = {*ruleset.find_edge("edge.stream"), *ruleset.find_edge("edge.road"),
                   *ruleset.find_edge("edge.river"), *ruleset.find_edge("edge.none")};
    return tiles;
}

}  // namespace aetheria::tests::site_projection_support

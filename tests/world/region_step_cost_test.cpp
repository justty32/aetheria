#include "core/world/region_movement.h"
#include "tests/world/region_test_support.h"

#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::plain_tiles;
using aetheria::tests::test_ruleset;
using aetheria::world::RegionXY;

TEST(RegionMovement, RoadRiverAndBridgeCostsComeFromEdgeDefinitions) {
    auto tiles = plain_tiles(2, 1);
    const RegionXY from{0, 0};
    const RegionXY to{1, 0};
    const auto plain = aetheria::world::region_step_cost(tiles, from, to, test_ruleset(), 1);
    const auto winter = aetheria::world::region_step_cost(tiles, from, to, test_ruleset(), 4);
    tiles.set_edge(from, to, *test_ruleset().find_edge("edge.road"));
    const auto road = aetheria::world::region_step_cost(tiles, from, to, test_ruleset(), 1);
    tiles.set_edge(from, to, *test_ruleset().find_edge("edge.river"));
    const auto river = aetheria::world::region_step_cost(tiles, from, to, test_ruleset(), 1);
    tiles.set_edge(from, to, *test_ruleset().find_edge("edge.bridge"));
    const auto bridge = aetheria::world::region_step_cost(tiles, from, to, test_ruleset(), 1);

    std::cout << "movement_cost plain=" << plain << " winter=" << winter << " road=" << road
              << " river=" << river << " bridge=" << bridge << '\n';
    EXPECT_GT(winter, plain);
    EXPECT_LT(road, plain);
    EXPECT_GT(river, plain);
    EXPECT_LT(bridge, river);
}

}  // namespace

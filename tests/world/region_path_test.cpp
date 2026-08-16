#include "core/world/region_movement.h"
#include "core/worldgen/region_generator.h"
#include "tests/world/region_test_support.h"

#include <cstddef>
#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::plain_tiles;
using aetheria::tests::test_ruleset;
using aetheria::world::find_region_path;
using aetheria::world::RegionXY;

TEST(RegionPathfinding, AStarMatchesDijkstraForAtLeastOneHundredRandomPairs) {
    auto tiles = plain_tiles(16, 12);
    const auto road = *test_ruleset().find_edge("edge.road");
    auto random = UINT64_C(123456789);
    for (std::int16_t y = 0; y < 12; ++y) {
        for (std::int16_t x = 0; x < 16; ++x) {
            const RegionXY here{x, y};
            if (x + 1 < 16) {
                random = aetheria::worldgen::splitmix64(random);
                if ((random & 3U) == 0) {
                    tiles.set_edge(here, RegionXY{static_cast<std::int16_t>(x + 1), y}, road);
                }
            }
            if (y + 1 < 12) {
                random = aetheria::worldgen::splitmix64(random);
                if ((random & 3U) == 0) {
                    tiles.set_edge(here, RegionXY{x, static_cast<std::int16_t>(y + 1)}, road);
                }
            }
        }
    }
    for (std::size_t sample = 0; sample < 128; ++sample) {
        random = aetheria::worldgen::splitmix64(random);
        const RegionXY start{static_cast<std::int16_t>(random % 16U),
                             static_cast<std::int16_t>((random >> 8U) % 12U)};
        random = aetheria::worldgen::splitmix64(random);
        const RegionXY goal{static_cast<std::int16_t>(random % 16U),
                            static_cast<std::int16_t>((random >> 8U) % 12U)};
        const auto astar = find_region_path(tiles, start, goal, test_ruleset(), 1, 1);
        const auto dijkstra = find_region_path(tiles, start, goal, test_ruleset(), 1, 0);
        ASSERT_EQ(astar.has_value(), dijkstra.has_value());
        ASSERT_TRUE(astar.has_value());
        EXPECT_EQ(astar->cost, dijkstra->cost) << "sample=" << sample;
    }
}

TEST(RegionPathfinding, DeliberatelyInadmissibleHeuristicProducesASuboptimalPath) {
    auto tiles = plain_tiles(5, 3);
    const auto road = *test_ruleset().find_edge("edge.road");
    tiles.set_edge({0, 1}, {0, 0}, road);
    for (std::int16_t x = 0; x < 4; ++x) {
        tiles.set_edge({x, 0}, {static_cast<std::int16_t>(x + 1), 0}, road);
    }
    tiles.set_edge({4, 0}, {4, 1}, road);
    const auto dijkstra = find_region_path(tiles, {0, 1}, {4, 1}, test_ruleset(), 1, 0);
    const auto astar = find_region_path(tiles, {0, 1}, {4, 1}, test_ruleset(), 1, 1);
    const auto weighted = find_region_path(tiles, {0, 1}, {4, 1}, test_ruleset(), 1, 100);

    ASSERT_TRUE(dijkstra && astar && weighted);
    EXPECT_EQ(astar->cost, dijkstra->cost);
    EXPECT_GT(weighted->cost, dijkstra->cost);
    std::cout << "heuristic_probe dijkstra=" << dijkstra->cost << " admissible=" << astar->cost
              << " weighted=" << weighted->cost << '\n';
}

}  // namespace

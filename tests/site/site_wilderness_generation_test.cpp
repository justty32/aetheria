#include "tests/site/site_wilderness_test_support.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <set>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::add_crossings;
using aetheria::tests::kWildCenter;
using aetheria::tests::kWildRegionId;
using aetheria::tests::kWildWorldSeed;
using aetheria::tests::nonzero_count;
using aetheria::tests::test_ruleset;
using aetheria::tests::wilderness_region;

[[nodiscard]] aetheria::site::SiteXY boundary_tile(aetheria::site::SiteBoundarySide side,
                                                   std::uint8_t position) {
    switch (side) {
    case aetheria::site::SiteBoundarySide::North:
        return {position, 0};
    case aetheria::site::SiteBoundarySide::East:
        return {aetheria::site::kSiteWidth - 1U, position};
    case aetheria::site::SiteBoundarySide::South:
        return {position, aetheria::site::kSiteHeight - 1U};
    case aetheria::site::SiteBoundarySide::West:
        return {0, position};
    }
    return {};
}

TEST(WildernessGeneration, W1ReliefControlsPassabilityAndTerrainHasSignal) {
    auto plain_region = wilderness_region();
    auto mountain_region = plain_region;
    mountain_region.relief[mountain_region.index_of(kWildCenter)] =
        *test_ruleset().find_relief("relief.mountain");
    const auto plain = aetheria::site::generate_wilderness_site(
        plain_region, kWildCenter, kWildWorldSeed, kWildRegionId, test_ruleset());
    const auto mountain = aetheria::site::generate_wilderness_site(
        mountain_region, kWildCenter, kWildWorldSeed, kWildRegionId, test_ruleset());
    const auto plain_passable = nonzero_count(plain.skeleton.terrain.buildable);
    const auto mountain_passable = nonzero_count(mountain.skeleton.terrain.buildable);
    const auto [minimum, maximum] =
        std::ranges::minmax_element(mountain.skeleton.terrain.elevation);
    EXPECT_LT(*minimum, *maximum);
    EXPECT_GT(plain_passable, mountain_passable);
    EXPECT_LT(mountain_passable, aetheria::site::kSiteTileCount / 2U);
    std::cout << "wild_relief passable_plain=" << plain_passable
              << " passable_mountain=" << mountain_passable
              << " mountain_height_span=" << (*maximum - *minimum) << '\n';
}

TEST(WildernessGeneration, W2AndW3ConnectEveryRiverAndRoadCrossing) {
    auto tiles = wilderness_region();
    add_crossings(tiles);
    const auto site = aetheria::site::generate_wilderness_site(
        tiles, kWildCenter, kWildWorldSeed, kWildRegionId, test_ruleset());
    EXPECT_EQ(site.skeleton.river_path_count, 1U);
    EXPECT_EQ(site.skeleton.road_path_count, 1U);
    EXPECT_GT(nonzero_count(site.skeleton.terrain.water), 64U);
    EXPECT_GT(nonzero_count(site.skeleton.terrain.roads), 64U);
    EXPECT_GT(site.skeleton.bridge_count, 0U);

    std::size_t river_crossings{};
    std::size_t road_crossings{};
    for (std::size_t side = 0; side < site.skeleton.boundaries.size(); ++side) {
        for (const auto& crossing : site.skeleton.boundaries[side].crossings) {
            const auto* edge = test_ruleset().edge(crossing.kind);
            const auto tile = boundary_tile(
                static_cast<aetheria::site::SiteBoundarySide>(side), crossing.pos);
            const auto index = static_cast<std::size_t>(tile.y) * aetheria::site::kSiteWidth +
                               tile.x;
            if ((edge->flags & aetheria::rules::kEdgeRiverFlag) != 0) {
                ++river_crossings;
                EXPECT_NE(site.skeleton.terrain.water[index], 0);
            }
            if ((edge->flags & aetheria::rules::kEdgeRoadFlag) != 0) {
                ++road_crossings;
                EXPECT_NE(site.skeleton.terrain.roads[index], 0);
            }
        }
    }
    EXPECT_EQ(river_crossings, 2U);
    EXPECT_EQ(road_crossings, 2U);
    std::cout << "wild_paths river_crossings=" << river_crossings
              << " river_paths=" << site.skeleton.river_path_count
              << " water_tiles=" << nonzero_count(site.skeleton.terrain.water)
              << " road_crossings=" << road_crossings
              << " road_paths=" << site.skeleton.road_path_count
              << " road_tiles=" << nonzero_count(site.skeleton.terrain.roads)
              << " bridge_tiles=" << site.skeleton.bridge_count << '\n';
}

TEST(WildernessGeneration, W4JitterGridHasAtMostOnePointPerCellAndForestIsDenser) {
    auto sparse_region = wilderness_region();
    auto forest_region = sparse_region;
    forest_region.feature[forest_region.index_of(kWildCenter)] =
        *test_ruleset().find_feature("feature.forest");
    const auto sparse = aetheria::site::generate_wilderness_site(
        sparse_region, kWildCenter, kWildWorldSeed, kWildRegionId, test_ruleset());
    const auto forest = aetheria::site::generate_wilderness_site(
        forest_region, kWildCenter, kWildWorldSeed, kWildRegionId, test_ruleset());
    const auto extent = test_ruleset().wilderness_generation_rules().jitter_cell_extent;
    std::set<std::pair<std::uint16_t, std::uint16_t>> occupied_cells;
    for (const auto tile : forest.skeleton.vegetation) {
        EXPECT_TRUE(occupied_cells.emplace(tile.x / extent, tile.y / extent).second);
    }
    EXPECT_GT(forest.skeleton.vegetation.size(), sparse.skeleton.vegetation.size() * 3U);
    std::cout << "wild_vegetation jitter_cells=" << occupied_cells.size()
              << " sparse_points=" << sparse.skeleton.vegetation.size()
              << " forest_points=" << forest.skeleton.vegetation.size() << '\n';
}

}  // namespace

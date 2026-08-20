#include "core/worldgen/influence_spread.h"
#include "core/world/region_movement.h"
#include "tests/worldgen/influence_test_support.h"
#include "tests/world/region_test_support.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::plain_tiles;
using aetheria::tests::owner_hash;
using aetheria::tests::sequential_first_wins;
using aetheria::tests::single_source_costs;
using aetheria::tests::test_ruleset;
using aetheria::world::FactionId;
using aetheria::world::RegionTiles;
using aetheria::world::RegionXY;
using aetheria::worldgen::CitySite;
using aetheria::worldgen::InfluenceCapital;
using aetheria::worldgen::InfluenceSpreadConfig;

TEST(InfluenceCapitalSelection, IsCanonicalUnderShuffleAndRejectsTooManyFactions) {
    std::vector<CitySite> cities{
        {40, {4, 4}, 80, aetheria::world::SettlementTier::Town, 2},
        {20, {0, 8}, 90, aetheria::world::SettlementTier::City, 4},
        {30, {8, 8}, 90, aetheria::world::SettlementTier::City, 4},
        {10, {8, 0}, 100, aetheria::world::SettlementTier::City, 4},
        {5, {0, 0}, 100, aetheria::world::SettlementTier::City, 4},
    };
    const auto selected_a = aetheria::worldgen::select_capitals(cities, 3);
    std::ranges::reverse(cities);
    const auto selected_b = aetheria::worldgen::select_capitals(cities, 3);

    ASSERT_EQ(selected_a, selected_b);
    ASSERT_EQ(selected_a.size(), 3U);
    EXPECT_EQ(selected_a[0].canonical_id, 5U);
    EXPECT_EQ(selected_a[1].canonical_id, 30U);
    EXPECT_EQ(selected_a[2].canonical_id, 10U);
    EXPECT_THROW(static_cast<void>(aetheria::worldgen::select_capitals(cities, 5)),
                 std::invalid_argument);
}

TEST(InfluenceSpread, CanonicalTieBreakIgnoresInputOrderAndNegativeControlDoesNot) {
    const auto tiles = plain_tiles(11, 7);
    std::vector<InfluenceCapital> capitals{{FactionId{1}, {2, 3}}, {FactionId{2}, {8, 3}}};
    const InfluenceSpreadConfig config{16, 1};
    const auto owners_a =
        aetheria::worldgen::spread_influence(tiles, capitals, test_ruleset(), config);
    const auto distances_a = single_source_costs(tiles, capitals[0].tile, config.max_cost);
    const auto distances_b = single_source_costs(tiles, capitals[1].tile, config.max_cost);
    const auto negative_a = sequential_first_wins(tiles, capitals, config.max_cost);
    std::ranges::reverse(capitals);
    const auto owners_b =
        aetheria::worldgen::spread_influence(tiles, capitals, test_ruleset(), config);
    const auto negative_b = sequential_first_wins(tiles, capitals, config.max_cost);

    std::size_t ties{};
    std::size_t unowned{};
    for (std::size_t index = 0; index < tiles.tile_count(); ++index) {
        const auto minimum = std::min(distances_a[index], distances_b[index]);
        ties += minimum <= config.max_cost && distances_a[index] == minimum &&
                distances_b[index] == minimum;
        unowned += owners_a[index] == FactionId{0};
    }
    std::cout << "influence canonical_a=" << owner_hash(owners_a)
              << " canonical_b=" << owner_hash(owners_b)
              << " negative_a=" << owner_hash(negative_a)
              << " negative_b=" << owner_hash(negative_b) << " tied_tiles=" << ties
              << " unowned=" << unowned << '/' << tiles.tile_count() << '\n';
    EXPECT_EQ(owners_a, owners_b);
    EXPECT_NE(negative_a, negative_b);
    EXPECT_GT(ties, 0U);
    EXPECT_GT(unowned, 0U);
}

TEST(InfluenceSpread, BoundaryTracksHighCostMountainRidge) {
    auto tiles = plain_tiles(15, 9);
    const auto mountain = *test_ruleset().find_relief("relief.mountain");
    for (std::int16_t y = 0; y < 9; ++y) {
        tiles.relief[tiles.index_of({7, y})] = mountain;
    }
    const std::array capitals{InfluenceCapital{FactionId{1}, {2, 4}},
                              InfluenceCapital{FactionId{2}, {12, 4}}};
    const auto owners = aetheria::worldgen::spread_influence(
        tiles, capitals, test_ruleset(), InfluenceSpreadConfig{100, 1});

    std::int64_t boundary_cost{};
    std::int64_t map_cost{};
    std::size_t boundary_tiles{};
    for (std::int16_t y = 0; y < 9; ++y) {
        for (std::int16_t x = 0; x < 15; ++x) {
            const RegionXY tile{x, y};
            const RegionXY neighbor{x == 0 ? std::int16_t{1} : static_cast<std::int16_t>(x - 1), y};
            const auto cost = aetheria::world::region_step_cost(
                tiles, neighbor, tile, test_ruleset(), 1);
            map_cost += cost;
            bool boundary{};
            if (x > 0) {
                boundary |= owners[tiles.index_of({static_cast<std::int16_t>(x - 1), y})] !=
                            owners[tiles.index_of(tile)];
            }
            if (x + 1 < 15) {
                boundary |= owners[tiles.index_of({static_cast<std::int16_t>(x + 1), y})] !=
                            owners[tiles.index_of(tile)];
            }
            if (boundary) {
                boundary_cost += cost;
                ++boundary_tiles;
            }
        }
    }
    const auto boundary_milli = boundary_cost * 1000 / static_cast<std::int64_t>(boundary_tiles);
    const auto map_milli = map_cost * 1000 / static_cast<std::int64_t>(tiles.tile_count());
    std::cout << "influence boundary_avg_milli_mp=" << boundary_milli
              << " map_avg_milli_mp=" << map_milli
              << " boundary_tiles=" << boundary_tiles << '\n';
    EXPECT_GT(boundary_milli * 100, map_milli * 140);
}

TEST(InfluenceSpread, OceanStopsExpansionAndLeavesFarShoreUnowned) {
    auto tiles = plain_tiles(5, 1);
    tiles.base[2] = *test_ruleset().find_terrain("terrain.ocean");
    const std::array capitals{InfluenceCapital{FactionId{1}, {0, 0}}};
    const auto owners = aetheria::worldgen::spread_influence(
        tiles, capitals, test_ruleset(), InfluenceSpreadConfig{100, 1});

    EXPECT_EQ(owners[0], FactionId{1});
    EXPECT_EQ(owners[1], FactionId{1});
    EXPECT_EQ(owners[2], FactionId{0});
    EXPECT_EQ(owners[3], FactionId{0});
    EXPECT_EQ(owners[4], FactionId{0});
}

}  // namespace

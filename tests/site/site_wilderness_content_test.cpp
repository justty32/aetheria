#include "tests/site/site_wilderness_test_support.h"
#include "tests/support/performance.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::add_crossings;
using aetheria::tests::kWildCenter;
using aetheria::tests::kWildRegionId;
using aetheria::tests::kWildWorldSeed;
using aetheria::tests::nonzero_count;
using aetheria::tests::test_ruleset;
using aetheria::tests::wilderness_region;

TEST(WildernessContent, W5OnlyFastOwnerChangesEncounterDensity) {
    auto tiles = wilderness_region();
    add_crossings(tiles);
    const auto index = tiles.index_of(kWildCenter);
    const auto slow = aetheria::site::project_wilderness_slow_vars(
        tiles, kWildCenter, kWildWorldSeed, kWildRegionId, test_ruleset());
    const auto seed = aetheria::site::derive_site_seed(kWildWorldSeed, kWildRegionId, 2, 2);
    const auto skeleton =
        aetheria::site::build_wilderness_skeleton(slow, seed, test_ruleset());
    auto fast = aetheria::site::split_site_vars(tiles, kWildCenter).fast;
    fast.owner = static_cast<aetheria::world::FactionId>(4);
    const auto owned =
        aetheria::site::populate_wilderness(skeleton, fast, seed, test_ruleset());
    fast.owner = static_cast<aetheria::world::FactionId>(0);
    fast.population = 999999;
    fast.development_level = 99;
    fast.defense = 100;
    fast.damage = 100;
    const auto unowned =
        aetheria::site::populate_wilderness(skeleton, fast, seed, test_ruleset());

    EXPECT_EQ(owned.skeleton, unowned.skeleton);
    EXPECT_EQ(owned.population.resource_points, unowned.population.resource_points);
    EXPECT_EQ(owned.population.traveler_points, unowned.population.traveler_points);
    EXPECT_EQ(owned.population.encounter_points.size(), 2U);
    EXPECT_EQ(unowned.population.encounter_points.size(), 6U);
    EXPECT_EQ(tiles.settlement[index], aetheria::world::SettlementTier::None);
    std::cout << "wild_w5 executed=1 resources=" << unowned.population.resource_points.size()
              << " encounters_owned=" << owned.population.encounter_points.size()
              << " encounters_unowned=" << unowned.population.encounter_points.size()
              << " travelers=" << unowned.population.traveler_points.size() << '\n';
}

TEST(WildernessContent, RuinRetainsStructuresAndHasMoreL3Entrances) {
    auto wild_region = wilderness_region();
    auto ruin_region = wild_region;
    ruin_region.feature[ruin_region.index_of(kWildCenter)] =
        *test_ruleset().find_feature("feature.ruin_city");
    const auto wild = aetheria::site::generate_wilderness_site(
        wild_region, kWildCenter, kWildWorldSeed, kWildRegionId, test_ruleset());
    const auto ruin = aetheria::site::generate_wilderness_site(
        ruin_region, kWildCenter, kWildWorldSeed, kWildRegionId, test_ruleset());
    EXPECT_TRUE(wild.skeleton.ruin_structures.empty());
    EXPECT_FALSE(ruin.skeleton.ruin_structures.empty());
    EXPECT_EQ(wild.skeleton.portals.size(), 1U);
    EXPECT_EQ(ruin.skeleton.portals.size(), 4U);
    EXPECT_GE(ruin.skeleton.ruin_structures.size(), 6U);
    EXPECT_LE(ruin.skeleton.ruin_structures.size(), 24U);
    std::cout << "wild_ruin retained_structures=" << ruin.skeleton.ruin_structures.size()
              << " wild_l3_portals=" << wild.skeleton.portals.size()
              << " ruin_l3_portals=" << ruin.skeleton.portals.size() << '\n';
}

TEST(WildernessContent, SeaUsesWaterEverywhereAndSkipsLandContent) {
    auto tiles = wilderness_region();
    std::ranges::fill(tiles.base, *test_ruleset().find_terrain("terrain.ocean"));
    const auto sea = aetheria::site::generate_wilderness_site(
        tiles, kWildCenter, kWildWorldSeed, kWildRegionId, test_ruleset());
    EXPECT_EQ(nonzero_count(sea.skeleton.terrain.water), aetheria::site::kSiteTileCount);
    EXPECT_TRUE(sea.skeleton.vegetation.empty());
    EXPECT_TRUE(sea.skeleton.portals.empty());
    EXPECT_TRUE(sea.population.resource_points.empty());
    EXPECT_FALSE(sea.population.encounter_points.empty());
}

TEST(WildernessContent, FullW1ThroughW6FitsThirtyMillisecondsWithBranchEvidence) {
    auto tiles = wilderness_region();
    add_crossings(tiles);
    const auto index = tiles.index_of(kWildCenter);
    tiles.feature[index] = *test_ruleset().find_feature("feature.ruin_city");
    tiles.relief[index] = *test_ruleset().find_relief("relief.mountain");
    aetheria::site::WildernessSite site;
    const auto minimum_milliseconds = aetheria::tests::minimum_milliseconds_after_warmup([&] {
        const auto start = std::chrono::steady_clock::now();
        site = aetheria::site::generate_wilderness_site(
            tiles, kWildCenter, kWildWorldSeed + 1U, kWildRegionId, test_ruleset());
        const auto elapsed = std::chrono::steady_clock::now() - start;
        return std::chrono::duration<double, std::milli>{elapsed}.count();
    });
    EXPECT_LT(minimum_milliseconds, 30.0);
    EXPECT_EQ(site.skeleton.terrain.elevation.size(), aetheria::site::kSiteTileCount);
    EXPECT_GT(site.skeleton.river_path_count, 0U);
    EXPECT_GT(site.skeleton.road_path_count, 0U);
    EXPECT_GT(site.skeleton.vegetation.size(), 0U);
    EXPECT_GT(site.population.resource_points.size(), 0U);
    EXPECT_GT(site.population.encounter_points.size(), 0U);
    EXPECT_GT(site.population.traveler_points.size(), 0U);
    EXPECT_GT(site.skeleton.portals.size(), 0U);
    EXPECT_GT(site.skeleton.ruin_structures.size(), 0U);
#ifdef NDEBUG
    std::cout << "wild_full_Release_min_of_5_ms=" << minimum_milliseconds;
#else
    std::cout << "wild_full_Debug_min_of_5_ms=" << minimum_milliseconds;
#endif
    std::cout << " timed_runs=5 W1_tiles=" << site.skeleton.terrain.elevation.size()
              << " W2_river_paths=" << site.skeleton.river_path_count
              << " W3_road_paths=" << site.skeleton.road_path_count
              << " W4_vegetation=" << site.skeleton.vegetation.size()
              << " W5_resources=" << site.population.resource_points.size()
              << " W5_encounters=" << site.population.encounter_points.size()
              << " W5_travelers=" << site.population.traveler_points.size()
              << " W6_portals=" << site.skeleton.portals.size()
              << " ruin_structures=" << site.skeleton.ruin_structures.size() << '\n';
}

}  // namespace

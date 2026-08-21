#include "tests/site/site_wilderness_test_support.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <type_traits>

#include <gtest/gtest.h>

namespace {

using aetheria::site::SiteBoundarySide;
using aetheria::site::WildernessSlowVars;
using aetheria::tests::actual_boundary;
using aetheria::tests::kWildRegionId;
using aetheria::tests::kWildWorldSeed;
using aetheria::tests::test_ruleset;
using aetheria::tests::wilderness_region;
using aetheria::world::RegionXY;

static_assert(!std::is_invocable_v<decltype(&aetheria::site::build_wilderness_skeleton),
                                   const aetheria::site::SiteFastVars&, std::uint64_t,
                                   const aetheria::rules::Ruleset&>);

TEST(WildernessBoundary, AdjacentSidesAreBitExactAndReachActualTerrain) {
    auto tiles = wilderness_region();
    constexpr RegionXY west{1, 2};
    constexpr RegionXY east{2, 2};
    tiles.set_edge(west, east, *test_ruleset().find_edge("edge.river"));
    tiles.base[tiles.index_of(east)] = *test_ruleset().find_terrain("terrain.desert");

    const auto west_slow = aetheria::site::project_wilderness_slow_vars(
        tiles, west, kWildWorldSeed, kWildRegionId, test_ruleset());
    const auto east_slow = aetheria::site::project_wilderness_slow_vars(
        tiles, east, kWildWorldSeed, kWildRegionId, test_ruleset());
    const auto& west_profile =
        west_slow.boundaries[static_cast<std::size_t>(SiteBoundarySide::East)];
    const auto& east_profile =
        east_slow.boundaries[static_cast<std::size_t>(SiteBoundarySide::West)];
    EXPECT_EQ(west_profile, east_profile);

    const auto west_site = aetheria::site::build_wilderness_skeleton(
        west_slow, aetheria::site::derive_site_seed(kWildWorldSeed, kWildRegionId, 1, 2),
        test_ruleset());
    const auto east_site = aetheria::site::build_wilderness_skeleton(
        east_slow, aetheria::site::derive_site_seed(kWildWorldSeed, kWildRegionId, 2, 2),
        test_ruleset());
    const auto west_actual =
        actual_boundary(west_site.terrain, SiteBoundarySide::East, west_profile.crossings);
    const auto east_actual =
        actual_boundary(east_site.terrain, SiteBoundarySide::West, east_profile.crossings);
    EXPECT_EQ(west_actual.elevation, west_profile.elevation);
    EXPECT_EQ(west_actual.ground, west_profile.ground);
    EXPECT_EQ(west_actual.water_depth, west_profile.water_depth);
    EXPECT_EQ(west_actual.edges, west_profile.edges);
    EXPECT_EQ(west_actual.crossings, west_profile.crossings);
    EXPECT_EQ(east_actual, east_profile);
    std::cout << "wild_boundary_shared samples=64 crossings=" << west_profile.crossings.size()
              << " elevation_first=" << west_profile.elevation.front()
              << " elevation_last=" << west_profile.elevation.back()
              << " bit_exact=elevation,ground,water_depth,edges,crossings\n";
}

TEST(WildernessBoundary, GenerationOrderAndAbsentNeighborSiteDoNotMatter) {
    auto tiles = wilderness_region();
    constexpr RegionXY west{1, 2};
    constexpr RegionXY east{2, 2};
    tiles.set_edge(west, east, *test_ruleset().find_edge("edge.highway"));
    const auto east_before = aetheria::site::generate_wilderness_site(
        tiles, east, kWildWorldSeed, kWildRegionId, test_ruleset());
    const auto west_after = aetheria::site::generate_wilderness_site(
        tiles, west, kWildWorldSeed, kWildRegionId, test_ruleset());
    const auto west_before = aetheria::site::generate_wilderness_site(
        tiles, west, kWildWorldSeed, kWildRegionId, test_ruleset());
    const auto east_after = aetheria::site::generate_wilderness_site(
        tiles, east, kWildWorldSeed, kWildRegionId, test_ruleset());

    EXPECT_EQ(east_before, east_after);
    EXPECT_EQ(west_before, west_after);
    EXPECT_EQ(east_before.skeleton.boundaries[static_cast<std::size_t>(SiteBoundarySide::West)],
              west_after.skeleton.boundaries[static_cast<std::size_t>(SiteBoundarySide::East)]);
}

TEST(WildernessBoundary, SharedCornerMatchesAcrossFourSites) {
    auto tiles = wilderness_region();
    constexpr std::array<RegionXY, 4> coordinates{
        RegionXY{1, 1}, RegionXY{2, 1}, RegionXY{1, 2}, RegionXY{2, 2}};
    std::array<aetheria::site::WildernessSite, 4> sites;
    for (std::size_t index = 0; index < sites.size(); ++index) {
        sites[index] = aetheria::site::generate_wilderness_site(
            tiles, coordinates[index], kWildWorldSeed, kWildRegionId, test_ruleset());
    }
    constexpr std::array<aetheria::site::SiteXY, 4> corners{
        aetheria::site::SiteXY{63, 63}, aetheria::site::SiteXY{0, 63},
        aetheria::site::SiteXY{63, 0}, aetheria::site::SiteXY{0, 0}};
    const auto sample = [&](std::size_t index) {
        const auto tile = corners[index];
        const auto offset = static_cast<std::size_t>(tile.y) * aetheria::site::kSiteWidth + tile.x;
        return std::pair{sites[index].skeleton.terrain.elevation[offset],
                         sites[index].skeleton.terrain.ground[offset]};
    };
    EXPECT_EQ(sample(0), sample(1));
    EXPECT_EQ(sample(0), sample(2));
    EXPECT_EQ(sample(0), sample(3));
}

TEST(WildernessBoundary, CanonicalDirectionIsNotMirrored) {
    auto tiles = wilderness_region();
    auto west = aetheria::site::project_wilderness_slow_vars(
        tiles, {1, 2}, kWildWorldSeed, kWildRegionId, test_ruleset());
    auto east = aetheria::site::project_wilderness_slow_vars(
        tiles, {2, 2}, kWildWorldSeed, kWildRegionId, test_ruleset());
    auto profile = west.boundaries[static_cast<std::size_t>(SiteBoundarySide::East)];
    const auto grass = *test_ruleset().find_ground("ground.grass");
    const auto none = *test_ruleset().find_edge("edge.none");
    profile.crossings.clear();
    for (std::size_t index = 0; index < profile.elevation.size(); ++index) {
        profile.elevation[index] = static_cast<std::uint16_t>(1000U + index * 4U);
        profile.ground[index] = grass;
        profile.water_depth[index] = 0;
        profile.edges[index] = none;
    }
    west.boundaries[static_cast<std::size_t>(SiteBoundarySide::East)] = profile;
    west.boundaries[static_cast<std::size_t>(SiteBoundarySide::South)].ground.back() = grass;
    west.boundaries[static_cast<std::size_t>(SiteBoundarySide::South)].elevation.back() =
        profile.elevation.back();
    west.boundaries[static_cast<std::size_t>(SiteBoundarySide::South)].water_depth.back() = 0;
    east.boundaries[static_cast<std::size_t>(SiteBoundarySide::West)] = profile;

    const auto west_site = aetheria::site::build_wilderness_skeleton(
        west, UINT64_C(0x111), test_ruleset());
    const auto east_site = aetheria::site::build_wilderness_skeleton(
        east, UINT64_C(0x222), test_ruleset());
    const auto west_actual = actual_boundary(west_site.terrain, SiteBoundarySide::East);
    const auto east_actual = actual_boundary(east_site.terrain, SiteBoundarySide::West);
    EXPECT_EQ(west_actual.elevation, profile.elevation);
    EXPECT_EQ(east_actual.elevation, profile.elevation);
    EXPECT_LT(west_actual.elevation.front(), west_actual.elevation.back());
    EXPECT_LT(east_actual.elevation.front(), east_actual.elevation.back());
}

}  // namespace

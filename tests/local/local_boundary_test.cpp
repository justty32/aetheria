#include "tests/local/local_test_support.h"
#include "tests/support/boundary_profile.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::local::kLocalWidth;
using aetheria::spatial::BoundarySide;
using aetheria::tests::hash_boundary_profile;
using aetheria::tests::kLocalSiteSeed;
using aetheria::tests::open_site_layer;
using aetheria::tests::set_site_edge;
using aetheria::tests::test_ruleset;

TEST(LocalBoundary, AdjacentLocalZonesShareProfileAndBoundaryWall) {
    auto parent = open_site_layer();
    constexpr aetheria::site::SiteXY west{31, 32};
    constexpr aetheria::site::SiteXY east{32, 32};
    const auto wall = *test_ruleset().find_edge("edge.city_wall");
    set_site_edge(parent, west, east, wall);

    const auto west_slow = aetheria::local::project_local_slow_vars(
        parent, west, kLocalSiteSeed, *test_ruleset().find_feature("feature.forest"),
        test_ruleset());
    const auto east_slow = aetheria::local::project_local_slow_vars(
        parent, east, kLocalSiteSeed, *test_ruleset().find_feature("feature.forest"),
        test_ruleset());
    const auto& west_profile =
        west_slow.boundaries[static_cast<std::size_t>(BoundarySide::East)];
    const auto& east_profile =
        east_slow.boundaries[static_cast<std::size_t>(BoundarySide::West)];
    EXPECT_EQ(west_profile, east_profile);
    EXPECT_EQ(std::ranges::count(west_profile.edges, wall), kLocalWidth);

    const auto west_local = aetheria::local::build_open_local_skeleton(
        west_slow, aetheria::local::derive_local_seed(kLocalSiteSeed, west.x, west.y),
        test_ruleset());
    const auto east_local = aetheria::local::build_open_local_skeleton(
        east_slow, aetheria::local::derive_local_seed(kLocalSiteSeed, east.x, east.y),
        test_ruleset());
    for (std::size_t position = 0; position < kLocalWidth; ++position) {
        const auto west_index = position * kLocalWidth + (kLocalWidth - 1U);
        const auto east_index = position * kLocalWidth;
        EXPECT_EQ(west_local.elevation[west_index], west_profile.elevation[position]);
        EXPECT_EQ(east_local.elevation[east_index], east_profile.elevation[position]);
        EXPECT_EQ(west_local.tiles.ground[west_index], east_local.tiles.ground[east_index]);
        EXPECT_EQ(west_local.tiles.edges
                      [west_index * 4U + static_cast<std::size_t>(BoundarySide::East)],
                  wall);
        EXPECT_EQ(east_local.tiles.edges
                      [east_index * 4U + static_cast<std::size_t>(BoundarySide::West)],
                  wall);
    }
    const auto profile_hash = hash_boundary_profile(west_profile);
    std::cout << "local_boundary_shared samples=64 wall_segments=64 hash=" << profile_hash
              << '\n';
    EXPECT_EQ(profile_hash, UINT64_C(3316258571901256250));
}

TEST(LocalBoundary, OrderAndAbsentNeighborDoNotMatter) {
    const auto parent = open_site_layer();
    constexpr aetheria::site::SiteXY west{31, 32};
    constexpr aetheria::site::SiteXY east{32, 32};
    const auto feature = *test_ruleset().find_feature("feature.none");
    const auto east_first = aetheria::local::project_local_slow_vars(
        parent, east, kLocalSiteSeed, feature, test_ruleset());
    const auto west_second = aetheria::local::project_local_slow_vars(
        parent, west, kLocalSiteSeed, feature, test_ruleset());
    const auto west_first = aetheria::local::project_local_slow_vars(
        parent, west, kLocalSiteSeed, feature, test_ruleset());
    const auto east_second = aetheria::local::project_local_slow_vars(
        parent, east, kLocalSiteSeed, feature, test_ruleset());
    EXPECT_EQ(east_first.boundaries, east_second.boundaries);
    EXPECT_EQ(west_first.boundaries, west_second.boundaries);
    EXPECT_EQ(east_first.boundaries[static_cast<std::size_t>(BoundarySide::West)],
              west_second.boundaries[static_cast<std::size_t>(BoundarySide::East)]);
}

TEST(LocalBoundary, SharedCornerMatchesAcrossFourLocalZones) {
    const auto parent = open_site_layer();
    constexpr std::array<aetheria::site::SiteXY, 4> coordinates{
        aetheria::site::SiteXY{31, 31}, aetheria::site::SiteXY{32, 31},
        aetheria::site::SiteXY{31, 32}, aetheria::site::SiteXY{32, 32}};
    constexpr std::array<aetheria::local::LocalXY, 4> corners{
        aetheria::local::LocalXY{63, 63}, aetheria::local::LocalXY{0, 63},
        aetheria::local::LocalXY{63, 0}, aetheria::local::LocalXY{0, 0}};
    const auto feature = *test_ruleset().find_feature("feature.none");
    std::array<aetheria::local::OpenLocalSkeleton, 4> locals;
    for (std::size_t index = 0; index < locals.size(); ++index) {
        const auto coordinate = coordinates[index];
        const auto slow = aetheria::local::project_local_slow_vars(
            parent, coordinate, kLocalSiteSeed, feature, test_ruleset());
        locals[index] = aetheria::local::build_open_local_skeleton(
            slow,
            aetheria::local::derive_local_seed(kLocalSiteSeed, coordinate.x, coordinate.y),
            test_ruleset());
    }
    const auto sample = [&](std::size_t index) {
        const auto tile = corners[index];
        const auto offset = static_cast<std::size_t>(tile.y) * kLocalWidth + tile.x;
        return std::pair{locals[index].elevation[offset], locals[index].tiles.ground[offset]};
    };
    EXPECT_EQ(sample(0), sample(1));
    EXPECT_EQ(sample(0), sample(2));
    EXPECT_EQ(sample(0), sample(3));
}

TEST(LocalBoundary, CanonicalDirectionIsNotMirrored) {
    const auto parent = open_site_layer();
    constexpr aetheria::site::SiteXY west{31, 32};
    constexpr aetheria::site::SiteXY east{32, 32};
    const auto feature = *test_ruleset().find_feature("feature.none");
    const auto west_slow = aetheria::local::project_local_slow_vars(
        parent, west, kLocalSiteSeed, feature, test_ruleset());
    const auto east_slow = aetheria::local::project_local_slow_vars(
        parent, east, kLocalSiteSeed, feature, test_ruleset());
    const auto& profile =
        west_slow.boundaries[static_cast<std::size_t>(BoundarySide::East)];
    ASSERT_NE(profile.elevation.front(), profile.elevation.back());
    EXPECT_EQ(profile,
              east_slow.boundaries[static_cast<std::size_t>(BoundarySide::West)]);
    const auto west_local = aetheria::local::build_open_local_skeleton(
        west_slow, aetheria::local::derive_local_seed(kLocalSiteSeed, west.x, west.y),
        test_ruleset());
    const auto east_local = aetheria::local::build_open_local_skeleton(
        east_slow, aetheria::local::derive_local_seed(kLocalSiteSeed, east.x, east.y),
        test_ruleset());
    for (std::size_t position = 0; position < kLocalWidth; ++position) {
        EXPECT_EQ(west_local.elevation[position * kLocalWidth + kLocalWidth - 1U],
                  profile.elevation[position]);
        EXPECT_EQ(east_local.elevation[position * kLocalWidth], profile.elevation[position]);
    }
}

}  // namespace

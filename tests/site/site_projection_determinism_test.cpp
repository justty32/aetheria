#include "core/serialize/all_components.h"
#include "core/site/site_projection.h"
#include "core/site/site_reduction.h"
#include "core/worldgen/region_seed.h"
#include "tests/site/site_projection_test_support.h"

#include <concepts>
#include <cstdint>
#include <iostream>
#include <type_traits>

#include <entt/core/type_traits.hpp>
#include <gtest/gtest.h>

namespace {

using aetheria::rules::Ruleset;
using aetheria::serialize::AllComponents;
using aetheria::serialize::SavedSiteLayers;
using aetheria::site::SiteFastVars;
using aetheria::site::SitePersistentLayer;
using aetheria::site::SiteProceduralLayer;
using aetheria::site::SiteSkeleton;
using aetheria::site::SiteSlowVars;
using aetheria::site::SiteVolatileLayer;
using aetheria::tests::test_ruleset;
using aetheria::world::RegionTiles;
using aetheria::world::RegionXY;

using aetheria::tests::site_projection_support::sample_region_tile;

TEST(SiteProjection, SeedFormulaAndSkeletonAreDeterministic) {
    constexpr auto world_seed = UINT64_C(20260821);
    constexpr std::uint32_t region_id = 7;
    constexpr std::uint16_t x = 17;
    constexpr std::uint16_t y = 32;
    const auto expected = aetheria::worldgen::splitmix64(
        world_seed ^ region_id ^ ((static_cast<std::uint64_t>(y) << 16U) | x));
    const auto site_seed = aetheria::site::derive_site_seed(world_seed, region_id, x, y);
    EXPECT_EQ(site_seed, expected);

    const auto vars = aetheria::site::split_site_vars(sample_region_tile(), RegionXY{0, 0});
    const auto first = aetheria::site::build_site_skeleton(vars.slow, site_seed, test_ruleset());
    const auto second = aetheria::site::build_site_skeleton(vars.slow, site_seed, test_ruleset());
    const auto other_seed = aetheria::site::derive_site_seed(world_seed, region_id, x + 1U, y);
    const auto other = aetheria::site::build_site_skeleton(vars.slow, other_seed, test_ruleset());
    const auto first_hash = aetheria::site::hash_site_skeleton(first);
    const auto second_hash = aetheria::site::hash_site_skeleton(second);
    const auto other_hash = aetheria::site::hash_site_skeleton(other);

    EXPECT_EQ(first, second);
    EXPECT_EQ(first_hash, second_hash);
    EXPECT_NE(first_hash, other_hash);
    std::cout << "site_determinism first=" << first_hash << " repeat=" << second_hash
              << " other_tile=" << other_hash << '\n';
}

}  // namespace

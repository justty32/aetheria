#include "core/serialize/all_components.h"
#include "core/site/site_projection.h"
#include "core/worldgen/region_seed.h"
#include "tests/support/ruleset_fixture.h"

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

template <typename Value>
concept HasOwner = requires(Value value) { value.owner; };
template <typename Value>
concept HasSettlement = requires(Value value) { value.settlement; };
template <typename Value>
concept HasSite = requires(Value value) { value.site; };
template <typename Value>
concept HasBase = requires(Value value) { value.base; };
template <typename Value>
concept HasRelief = requires(Value value) { value.relief; };
template <typename Value>
concept HasElevation = requires(Value value) { value.elevation; };

static_assert(HasBase<SiteSlowVars> && HasRelief<SiteSlowVars> && HasElevation<SiteSlowVars>);
static_assert(!HasOwner<SiteSlowVars> && !HasSettlement<SiteSlowVars> && !HasSite<SiteSlowVars>);
static_assert(HasOwner<SiteFastVars> && HasSettlement<SiteFastVars> && HasSite<SiteFastVars>);
static_assert(!HasBase<SiteFastVars> && !HasRelief<SiteFastVars> && !HasElevation<SiteFastVars>);
static_assert(std::is_invocable_r_v<SiteSkeleton, decltype(&aetheria::site::build_site_skeleton),
                                    const SiteSlowVars&, std::uint64_t, const Ruleset&>);
static_assert(!std::is_invocable_v<decltype(&aetheria::site::build_site_skeleton),
                                   const SiteFastVars&, std::uint64_t, const Ruleset&>);
static_assert(SavedSiteLayers::size == 1);
static_assert(entt::type_list_contains_v<SavedSiteLayers, SitePersistentLayer>);
static_assert(!entt::type_list_contains_v<SavedSiteLayers, SiteProceduralLayer>);
static_assert(!entt::type_list_contains_v<SavedSiteLayers, SiteVolatileLayer>);
static_assert(!entt::type_list_contains_v<AllComponents, SiteProceduralLayer>);

[[nodiscard]] RegionTiles sample_region_tile() {
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

[[nodiscard]] std::uint64_t skeleton_hash(const RegionTiles& tiles, std::uint64_t seed) {
    const auto vars = aetheria::site::split_site_vars(tiles, RegionXY{0, 0});
    return aetheria::site::hash_site_skeleton(
        aetheria::site::build_site_skeleton(vars.slow, seed, test_ruleset()));
}

TEST(SiteProjection, FastVariablesCannotAffectSkeleton) {
    constexpr auto seed = UINT64_C(0x5A17E);
    auto tiles = sample_region_tile();
    const auto baseline = skeleton_hash(tiles, seed);

    tiles.owner[0] = static_cast<aetheria::world::FactionId>(2);
    const auto changed_owner = skeleton_hash(tiles, seed);
    tiles.settlement[0] = aetheria::world::SettlementTier::City;
    const auto changed_settlement = skeleton_hash(tiles, seed);
    tiles.site[0].ever_realized = true;
    const auto changed_site = skeleton_hash(tiles, seed);

    EXPECT_EQ(changed_owner, baseline);
    EXPECT_EQ(changed_settlement, baseline);
    EXPECT_EQ(changed_site, baseline);
    std::cout << "site_fast_control baseline=" << baseline << " owner=" << changed_owner
              << " settlement=" << changed_settlement << " site=" << changed_site << '\n';
}

TEST(SiteProjection, EachRequiredSlowVariableChangesSkeleton) {
    constexpr auto seed = UINT64_C(0x5A17E);
    auto tiles = sample_region_tile();
    const auto baseline = skeleton_hash(tiles, seed);

    tiles.base[0] = *test_ruleset().find_terrain("terrain.desert");
    const auto changed_base = skeleton_hash(tiles, seed);
    tiles = sample_region_tile();
    tiles.relief[0] = *test_ruleset().find_relief("relief.hills");
    const auto changed_relief = skeleton_hash(tiles, seed);
    tiles = sample_region_tile();
    tiles.elevation[0] = 12300;
    const auto changed_elevation = skeleton_hash(tiles, seed);
    tiles = sample_region_tile();
    tiles.feature[0] = *test_ruleset().find_feature("feature.forest");
    const auto changed_feature = skeleton_hash(tiles, seed);
    tiles = sample_region_tile();
    tiles.edges[1] = *test_ruleset().find_edge("edge.none");
    const auto changed_edges = skeleton_hash(tiles, seed);

    EXPECT_NE(changed_base, baseline);
    EXPECT_NE(changed_relief, baseline);
    EXPECT_NE(changed_elevation, baseline);
    EXPECT_NE(changed_feature, baseline);
    EXPECT_NE(changed_edges, baseline);
    std::cout << "site_slow_control baseline=" << baseline << " base=" << changed_base
              << " relief=" << changed_relief << " elevation=" << changed_elevation
              << " feature=" << changed_feature << " edges=" << changed_edges << '\n';
}

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

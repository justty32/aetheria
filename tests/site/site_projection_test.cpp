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

template <typename Value>
concept HasOwner = requires(Value value) { value.owner; };
template <typename Value>
concept HasSettlement = requires(Value value) { value.settlement; };
template <typename Value>
concept HasSite = requires(Value value) { value.site; };
template <typename Value>
concept HasFillVars = requires(Value value) {
    value.population;
    value.development_level;
    value.defense;
    value.damage;
};
template <typename Value>
concept HasBase = requires(Value value) { value.base; };
template <typename Value>
concept HasRelief = requires(Value value) { value.relief; };
template <typename Value>
concept HasElevation = requires(Value value) { value.elevation; };

static_assert(HasBase<SiteSlowVars> && HasRelief<SiteSlowVars> && HasElevation<SiteSlowVars>);
static_assert(!HasOwner<SiteSlowVars> && !HasSettlement<SiteSlowVars> && !HasSite<SiteSlowVars>);
static_assert(!HasFillVars<SiteSlowVars>);
static_assert(HasOwner<SiteFastVars> && HasSettlement<SiteFastVars> && HasSite<SiteFastVars> &&
              HasFillVars<SiteFastVars>);
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

using aetheria::tests::site_projection_support::sample_region_tile;

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
    tiles.defense[0] = 100;
    const auto changed_defense = skeleton_hash(tiles, seed);
    tiles.damage[0] = 75;
    const auto changed_damage = skeleton_hash(tiles, seed);
    aetheria::site::SiteLayers measured;
    measured.persistent.buildings.resize(10, {{1, 1},
                                              aetheria::site::BuildingType::SettlementHall,
                                              aetheria::site::BuildingState::Active});
    aetheria::site::ReductionTable::apply(tiles, RegionXY{0, 0},
                                          aetheria::site::ReductionTable::reduce(measured));
    const auto changed_population_development = skeleton_hash(tiles, seed);
    const auto projected_fast = aetheria::site::split_site_vars(tiles, RegionXY{0, 0}).fast;

    EXPECT_EQ(changed_owner, baseline);
    EXPECT_EQ(changed_settlement, baseline);
    EXPECT_EQ(changed_site, baseline);
    EXPECT_EQ(changed_defense, baseline);
    EXPECT_EQ(changed_damage, baseline);
    EXPECT_EQ(changed_population_development, baseline);
    EXPECT_EQ(projected_fast.population, 1000U);
    EXPECT_EQ(projected_fast.development_level, 10U);
    EXPECT_EQ(projected_fast.defense, 100U);
    EXPECT_EQ(projected_fast.damage, 75U);
    std::cout << "site_fast_control baseline=" << baseline << " owner=" << changed_owner
              << " settlement=" << changed_settlement << " site=" << changed_site
              << " defense=" << changed_defense << " damage=" << changed_damage
              << " population_development=" << changed_population_development << '\n';
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

}  // namespace

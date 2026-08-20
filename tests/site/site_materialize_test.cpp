#include "core/site/site_materialize.h"

#include "core/serialize/all_components.h"
#include "core/serialize/zone_codec.h"
#include "core/site/site_projection.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
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
using aetheria::site::BuildingState;
using aetheria::site::SiteFastVars;
using aetheria::site::SitePersistentLayer;
using aetheria::site::SiteProceduralLayer;
using aetheria::site::SiteSkeleton;
using aetheria::site::SiteSlowVars;
using aetheria::site::SiteVolatileLayer;
using aetheria::tests::test_ruleset;
using aetheria::world::RegionTiles;
using aetheria::world::RegionXY;

static_assert(std::is_invocable_r_v<SiteProceduralLayer, decltype(&aetheria::site::populate),
                                    SiteSkeleton, const SiteFastVars&>);
static_assert(
    !std::is_invocable_v<decltype(&aetheria::site::populate), SiteSkeleton, const SiteSlowVars&>);
static_assert(SavedSiteLayers::size == 1);
static_assert(entt::type_list_contains_v<SavedSiteLayers, SitePersistentLayer>);
static_assert(!entt::type_list_contains_v<SavedSiteLayers, SiteProceduralLayer>);
static_assert(!entt::type_list_contains_v<SavedSiteLayers, SiteVolatileLayer>);
static_assert(!entt::type_list_contains_v<AllComponents, SitePersistentLayer>);

constexpr std::uint64_t kWorldSeed = UINT64_C(0xA37E1222);
constexpr std::uint32_t kRegionId = 7;
constexpr RegionXY kCoordinate{4, 7};

[[nodiscard]] RegionTiles sample_region() {
    const auto& ruleset = test_ruleset();
    RegionTiles tiles{8, 8};
    std::ranges::fill(tiles.base, *ruleset.find_terrain("terrain.grassland"));
    std::ranges::fill(tiles.relief, *ruleset.find_relief("relief.plain"));
    std::ranges::fill(tiles.feature, *ruleset.find_feature("feature.none"));
    std::ranges::fill(tiles.edges, *ruleset.find_edge("edge.none"));
    const auto index = tiles.index_of(kCoordinate);
    tiles.owner[index] = static_cast<aetheria::world::FactionId>(2);
    tiles.settlement[index] = aetheria::world::SettlementTier::Town;
    return tiles;
}

[[nodiscard]] std::size_t settled_tiles(const SiteProceduralLayer& layer) {
    return static_cast<std::size_t>(
        std::ranges::count(layer.zoning, aetheria::site::SiteZoning::Settlement));
}

TEST(SitePopulate, FastSettlementChangesZoningWithoutChangingSkeleton) {
    const auto tiles = sample_region();
    auto vars = aetheria::site::split_site_vars(tiles, kCoordinate);
    const auto seed = aetheria::site::derive_site_seed(kWorldSeed, kRegionId, 4, 7);
    const auto skeleton = aetheria::site::build_site_skeleton(vars.slow, seed, test_ruleset());

    vars.fast.settlement = aetheria::world::SettlementTier::Village;
    const auto village = aetheria::site::populate(skeleton, vars.fast);
    vars.fast.settlement = aetheria::world::SettlementTier::City;
    const auto city = aetheria::site::populate(skeleton, vars.fast);

    EXPECT_EQ(village.skeleton, city.skeleton);
    EXPECT_LT(settled_tiles(village), settled_tiles(city));
}

TEST(SiteMaterialize, DerivesZoneKeyAndCreatesOnePersistentBuildingAtCoarseLod) {
    auto site = aetheria::site::materialize_site_zone(sample_region(), kCoordinate, kWorldSeed,
                                                      kRegionId, test_ruleset());
    const auto expected_region = aetheria::zone::child_key(aetheria::zone::kRootZone, kRegionId, 0);
    const auto expected_site = aetheria::zone::child_key(expected_region, 4, 7);
    EXPECT_EQ(site.key, expected_site);
    EXPECT_EQ(aetheria::zone::region_id_of(site.key), kRegionId);
    EXPECT_EQ(aetheria::zone::site_x_of(site.key), 4U);
    EXPECT_EQ(aetheria::zone::site_y_of(site.key), 7U);
    EXPECT_EQ(site.lod, aetheria::zone::LodLevel::Coarse);

    const auto& layers = std::get<aetheria::zone::SitePayload>(site.payload).layers;
    ASSERT_TRUE(layers.procedural.valid_layout());
    ASSERT_EQ(layers.persistent.buildings.size(), 1U);
    EXPECT_TRUE(layers.procedural.skeleton.is_buildable(layers.persistent.buildings.front().tile));
}

TEST(SiteMaterialize, PersistentBuildingRoundTripsButProceduralLayerDoesNotSave) {
    auto source = aetheria::site::materialize_site_zone(sample_region(), kCoordinate, kWorldSeed,
                                                        kRegionId, test_ruleset());
    auto& source_layers = std::get<aetheria::zone::SitePayload>(source.payload).layers;
    source_layers.persistent.buildings.front().state = BuildingState::Derelict;
    const auto expected = source_layers.persistent;

    const auto bytes = aetheria::serialize::encode_zone(source, test_ruleset());
    auto loaded = aetheria::serialize::decode_zone(bytes, test_ruleset());
    const auto& loaded_layers = std::get<aetheria::zone::SitePayload>(loaded->payload).layers;
    EXPECT_EQ(loaded_layers.persistent, expected);
    EXPECT_TRUE(loaded_layers.procedural.skeleton.ground.empty());
    EXPECT_TRUE(loaded_layers.procedural.zoning.empty());
}

TEST(SiteMaterialize, BuildingCoordinateRemainsBuildableAfterFastVariableChanges) {
    auto tiles = sample_region();
    auto materialized = aetheria::site::materialize_site_zone(tiles, kCoordinate, kWorldSeed,
                                                              kRegionId, test_ruleset());
    const auto& original_layers =
        std::get<aetheria::zone::SitePayload>(materialized.payload).layers;
    const auto building_tile = original_layers.persistent.buildings.front().tile;
    const auto original_hash =
        aetheria::site::hash_site_skeleton(original_layers.procedural.skeleton);

    const auto index = tiles.index_of(kCoordinate);
    tiles.owner[index] = static_cast<aetheria::world::FactionId>(5);
    tiles.settlement[index] = aetheria::world::SettlementTier::City;
    tiles.site[index].ever_realized = true;
    const auto vars = aetheria::site::split_site_vars(tiles, kCoordinate);
    const auto seed = aetheria::site::derive_site_seed(kWorldSeed, kRegionId, 4, 7);
    const auto projected = aetheria::site::populate(
        aetheria::site::build_site_skeleton(vars.slow, seed, test_ruleset()), vars.fast);
    const auto projected_hash = aetheria::site::hash_site_skeleton(projected.skeleton);

    std::cout << "site_coordinate_stability x=" << building_tile.x << " y=" << building_tile.y
              << " original_skeleton=" << original_hash
              << " reprojected_skeleton=" << projected_hash
              << " buildable=" << projected.skeleton.is_buildable(building_tile) << '\n';
    EXPECT_EQ(projected_hash, original_hash);
    EXPECT_TRUE(projected.skeleton.is_buildable(building_tile));
}

}  // namespace

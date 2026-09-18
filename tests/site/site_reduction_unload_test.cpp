#include "core/serialize/zone_codec.h"
#include "core/site/site_materialize.h"
#include "core/site/site_reduction.h"
#include "core/world/region_movement.h"
#include "core/world/region_simulation.h"
#include "tests/site/site_reduction_test_support.h"

#include <algorithm>
#include <iostream>
#include <tuple>
#include <type_traits>

#include <gtest/gtest.h>

namespace {

using aetheria::site::BuildingState;
using aetheria::tests::kReductionCoordinate;
using aetheria::tests::kReductionRegionId;
using aetheria::tests::kReductionWorldSeed;
using aetheria::tests::reduction_region;
using aetheria::tests::test_ruleset;
using aetheria::world::DevelopmentLevelReduction;
using aetheria::world::OrderReduction;
using aetheria::world::PopulationReduction;
using aetheria::world::RegionSimulation;
using aetheria::world::RegionTiles;

TEST(SiteReduction, CollapseAlwaysReducesBeforeUnloadAndCurrentFormatPersistsFastFields) {
    auto tiles = reduction_region();
    tiles.defense[0] = 87;
    tiles.damage[0] = 42;
    aetheria::zone::InMemoryZoneStore store{test_ruleset()};
    auto first = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, test_ruleset());
    store.save(first);
    tiles.site[0].lod = aetheria::zone::LodLevel::Absent;
    tiles.site[0].has_live_site = false;

    aetheria::zone::ZoneManager manager{store};
    const auto handle = aetheria::site::rematerialize_site_zone(
        manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
        test_ruleset());
    ASSERT_TRUE(manager.with(handle, [](aetheria::zone::Zone& site) {
        std::get<aetheria::zone::SitePayload>(site.payload)
            .layers.persistent.buildings.front().state = BuildingState::Derelict;
    }));
    aetheria::site::collapse_site_zone(manager, handle, tiles, kReductionCoordinate,
                                       test_ruleset());
    EXPECT_FALSE(manager.get(handle.key()).has_value());
    EXPECT_FALSE(tiles.site[0].has_live_site);
    EXPECT_EQ(tiles.reduction_value<PopulationReduction>(kReductionCoordinate), 25U);
    EXPECT_EQ(tiles.reduction_value<DevelopmentLevelReduction>(kReductionCoordinate), 0U);

    aetheria::zone::Zone region_zone{
        aetheria::zone::child_key(aetheria::zone::kRootZone, kReductionRegionId, 0)};
    std::get<aetheria::zone::RegionPayload>(region_zone.payload).layers.emplace(0,
                                                                                 std::move(tiles));
    const auto bytes = aetheria::serialize::encode_zone(region_zone, test_ruleset());
    auto loaded = aetheria::serialize::decode_zone(bytes, test_ruleset());
    const auto& loaded_tiles =
        std::get<aetheria::zone::RegionPayload>(loaded->payload).layers.at(0);
    EXPECT_EQ(loaded_tiles.reduction_value<PopulationReduction>(kReductionCoordinate), 25U);
    EXPECT_EQ(loaded_tiles.reduction_value<DevelopmentLevelReduction>(kReductionCoordinate), 0U);
    EXPECT_EQ(loaded_tiles.reduction_value<OrderReduction>(kReductionCoordinate), 20U);
    EXPECT_EQ(loaded_tiles.defense[0], 87U);
    EXPECT_EQ(loaded_tiles.damage[0], 42U);
    EXPECT_FALSE(loaded_tiles.site[0].has_live_site);
    std::cout << "collapse_reduction population=25 development=0 has_live_site=0 "
                 "format_v="
              << aetheria::serialize::kSaveFormatVersion << '\n';
}

}  // namespace

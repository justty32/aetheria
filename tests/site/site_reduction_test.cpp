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

template <typename Value>
concept HasPublicPopulationField = requires(Value value) { value.population; };

static_assert(std::tuple_size_v<aetheria::world::RegionReductionRows> == 5);
static_assert(!HasPublicPopulationField<RegionTiles>);
static_assert(!std::is_default_constructible_v<aetheria::world::RegionTileDelta>);

TEST(SiteReduction, FixedRowsAreTheOnlySiteSideWriter) {
    auto tiles = reduction_region();
    auto live_site = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, test_ruleset());
    aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, live_site);
    EXPECT_EQ(tiles.reduction_value<PopulationReduction>(kReductionCoordinate), 100U);
    EXPECT_EQ(tiles.reduction_value<DevelopmentLevelReduction>(kReductionCoordinate), 1U);

    auto& layers = std::get<aetheria::zone::SitePayload>(live_site.payload).layers;
    layers.persistent.buildings.front().state = BuildingState::Idle;
    aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, live_site);
    const auto before_unlisted_change =
        tiles.reduction_value<PopulationReduction>(kReductionCoordinate);
    std::ranges::fill(layers.procedural.zoning, aetheria::site::SiteZoning::Open);
    aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, live_site);
    EXPECT_EQ(tiles.reduction_value<PopulationReduction>(kReductionCoordinate),
              before_unlisted_change);
    EXPECT_EQ(tiles.reduction_value<PopulationReduction>(kReductionCoordinate), 75U);
    EXPECT_EQ(tiles.reduction_value<DevelopmentLevelReduction>(kReductionCoordinate), 1U);
}

TEST(SiteReduction, MissingOrderObservationIsDistinctFromObservedZero) {
    auto tiles = reduction_region();
    aetheria::site::SiteLayers missing;
    const auto missing_delta = aetheria::site::ReductionTable::reduce(missing);
    EXPECT_FALSE(missing_delta.value<OrderReduction>().has_value());

    aetheria::site::SiteLayers anarchy;
    anarchy.persistent.order = aetheria::site::SiteOrderState{
        .garrison_coverage = 10,
        .patrol_coverage = 5,
        .bandit_pressure = 12,
        .refugee_pressure = 3,
    };
    const auto zero_delta = aetheria::site::ReductionTable::reduce(anarchy);
    ASSERT_TRUE(zero_delta.value<OrderReduction>().has_value());
    EXPECT_EQ(*zero_delta.value<OrderReduction>(), 0U);

    aetheria::site::SiteLayers retained_source;
    retained_source.persistent.order = aetheria::site::SiteOrderState{
        .garrison_coverage = 73,
    };
    aetheria::site::ReductionTable::apply(tiles, kReductionCoordinate,
                                          aetheria::site::ReductionTable::reduce(retained_source));
    aetheria::site::ReductionTable::apply(tiles, kReductionCoordinate, missing_delta);
    EXPECT_EQ(tiles.reduction_value<OrderReduction>(kReductionCoordinate), 73U);
    aetheria::site::ReductionTable::apply(tiles, kReductionCoordinate, zero_delta);
    EXPECT_EQ(tiles.reduction_value<OrderReduction>(kReductionCoordinate), 0U);

    std::cout << "order_optional missing_has_value=0 observed_zero_has_value=1 "
                 "retained_after_missing=73 overwritten_after_zero=0\n";
}

TEST(SiteReduction, HasLiveSiteNegativeControlProvesRegionFormulaDidNotExecute) {
    auto tiles = reduction_region();
    auto live_site = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, test_ruleset());
    auto& building = std::get<aetheria::zone::SitePayload>(live_site.payload)
                         .layers.persistent.buildings.front();
    building.state = BuildingState::Idle;
    aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, live_site);

    const auto skipped = RegionSimulation::advance_xun(tiles);
    EXPECT_EQ(skipped.formula_execution_count, 0U);
    EXPECT_EQ(skipped.live_site_skip_count, 1U);
    EXPECT_EQ(tiles.reduction_value<PopulationReduction>(kReductionCoordinate), 75U);

    tiles.site[0].has_live_site = false;
    const auto executed = RegionSimulation::advance_xun(tiles);
    EXPECT_EQ(executed.formula_execution_count, 1U);
    EXPECT_EQ(executed.live_site_skip_count, 0U);
    EXPECT_EQ(tiles.reduction_value<PopulationReduction>(kReductionCoordinate), 78U);
    std::cout << "has_live_site_negative formula_when_live="
              << skipped.formula_execution_count << " skipped_when_live="
              << skipped.live_site_skip_count << " formula_when_absent="
              << executed.formula_execution_count << " site_value=75 region_value=78\n";
}

TEST(SiteReduction, RegionTurnRequiresAndRunsOneLiveSiteReductionPassPerXun) {
    const auto region_key =
        aetheria::zone::child_key(aetheria::zone::kRootZone, kReductionRegionId, 0);
    aetheria::zone::Zone region{region_key};
    auto& tiles = std::get<aetheria::zone::RegionPayload>(region.payload)
                      .layers.emplace(0, reduction_region()).first->second;
    auto live_site = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, test_ruleset());
    const auto placeholder = *region.reg.view<aetheria::zone::ZoneMeta>().begin();
    region.reg.emplace<aetheria::world::TurnClock>(placeholder);
    aetheria::zone::InMemoryZoneStore store{test_ruleset()};
    aetheria::world::RegionTurnPipeline pipeline{test_ruleset(), store};

    EXPECT_THROW(pipeline.advance_xun(region), std::logic_error);
    std::size_t reduction_passes{};
    pipeline.advance_xun(region, {}, [&](aetheria::zone::Zone& reducing_region) {
        ++reduction_passes;
        auto& reducing_tiles = std::get<aetheria::zone::RegionPayload>(reducing_region.payload)
                                   .layers.at(0);
        aetheria::site::reduce_live_site_xun(reducing_tiles, kReductionCoordinate, live_site);
    });
    EXPECT_EQ(reduction_passes, 1U);
    EXPECT_EQ(tiles.reduction_value<PopulationReduction>(kReductionCoordinate), 100U);
}

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
    aetheria::site::collapse_site_zone(manager, handle, tiles, kReductionCoordinate);
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

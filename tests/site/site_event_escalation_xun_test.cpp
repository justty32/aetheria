#include "core/site/site_event_escalation.h"
#include "core/site/site_materialize.h"
#include "core/site/site_reduction.h"
#include "core/world/region_movement.h"
#include "core/zone/file_zone_store.h"
#include "sim/world_hash.h"
#include "tests/site/site_reduction_test_support.h"
#include "tests/support/performance.h"
#include "tests/zone/zone_test_support.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <type_traits>

#include <gtest/gtest.h>

namespace {

using aetheria::site::BuildingState;
using aetheria::site::PersistentBuilding;
using aetheria::site::SiteBuildingStateEvent;
using aetheria::tests::kReductionCoordinate;
using aetheria::tests::kReductionRegionId;
using aetheria::tests::kReductionWorldSeed;
using aetheria::tests::reduction_region;
using aetheria::tests::test_ruleset;
using aetheria::world::DevelopmentLevelReduction;
using aetheria::world::PopulationReduction;
using aetheria::world::Significance;

static_assert(std::is_same_v<decltype(SiteBuildingStateEvent::significance), Significance>);

[[nodiscard]] aetheria::site::SiteXY another_buildable_tile(
    const aetheria::site::SiteLayers& layers, aetheria::site::SiteXY occupied) {
    for (std::size_t index = 0; index < aetheria::site::kSiteTileCount; ++index) {
        const aetheria::site::SiteXY candidate{
            static_cast<std::uint16_t>(index % aetheria::site::kSiteWidth),
            static_cast<std::uint16_t>(index / aetheria::site::kSiteWidth)};
        if (candidate != occupied && layers.procedural.skeleton.is_buildable(candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error{"測試 Site 找不到第二個可建格"};
}

TEST(SiteEventEscalation, SiteSignificanceWaitsForTheXunReduction) {
    auto tiles = reduction_region();
    auto live_site = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, test_ruleset());
    aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, live_site, test_ruleset());
    auto& layers = std::get<aetheria::zone::SitePayload>(live_site.payload).layers;
    const SiteBuildingStateEvent event{Significance::Site,
                                       layers.persistent.buildings.front().tile,
                                       BuildingState::Idle};

    const bool escalated = aetheria::site::apply_site_building_state_event(
        tiles, kReductionCoordinate, live_site, event, test_ruleset());
    const auto before_xun = tiles.reduction_value<PopulationReduction>(kReductionCoordinate);
    aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, live_site, test_ruleset());
    const auto after_xun = tiles.reduction_value<PopulationReduction>(kReductionCoordinate);

    EXPECT_FALSE(escalated);
    EXPECT_EQ(before_xun, 100U);
    EXPECT_EQ(after_xun, 75U);
    std::cout << "event_site_negative escalated=0 before_xun=" << before_xun
              << " after_xun=" << after_xun << '\n';
}

TEST(SiteEventEscalation, XunSnapshotKeepsOtherChangeWithoutCountingEscalationTwice) {
    auto tiles = reduction_region();
    auto live_site = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, test_ruleset());
    auto& layers = std::get<aetheria::zone::SitePayload>(live_site.payload).layers;
    const auto event_tile = layers.persistent.buildings.front().tile;
    const auto other_tile = another_buildable_tile(layers, event_tile);
    layers.persistent.buildings.push_back(
        PersistentBuilding{other_tile, aetheria::site::BuildingType::SettlementHall,
                           BuildingState::Active});
    aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, live_site, test_ruleset());
    const auto baseline = tiles.reduction_value<PopulationReduction>(kReductionCoordinate);

    const bool escalated = aetheria::site::apply_site_building_state_event(
        tiles, kReductionCoordinate, live_site,
        SiteBuildingStateEvent{Significance::Region, event_tile, BuildingState::Idle},
        test_ruleset());
    const auto after_event = tiles.reduction_value<PopulationReduction>(kReductionCoordinate);
    layers.persistent.buildings.back().state = BuildingState::Idle;
    aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, live_site, test_ruleset());
    const auto after_xun = tiles.reduction_value<PopulationReduction>(kReductionCoordinate);

    const auto event_effect = baseline - after_event;
    const auto other_xun_effect = after_event - after_xun;
    const auto double_counted = after_xun - event_effect;
    EXPECT_TRUE(escalated);
    EXPECT_GT(event_effect, 0U);
    EXPECT_GT(other_xun_effect, 0U);
    EXPECT_EQ(baseline, 200U);
    EXPECT_EQ(after_event, 175U);
    EXPECT_EQ(after_xun, 150U);
    EXPECT_EQ(double_counted, 125U);
    EXPECT_NE(after_xun, double_counted);
    std::cout << "event_no_double_count baseline=" << baseline
              << " event_effect=" << event_effect << " after_event=" << after_event
              << " other_xun_effect=" << other_xun_effect << " final=" << after_xun
              << " double_count_counterfactual=" << double_counted << '\n';
}

}  // namespace

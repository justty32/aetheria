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

TEST(SiteEventEscalation, RegionSignificanceChangesRegionImmediatelyAndWorldHashSeesIt) {
    aetheria::tests::TemporaryDirectory directory;
    const auto raws_hash = aetheria::tests::prepare_test_save_raws(directory.path());
    auto tiles = reduction_region();
    tiles.owner[0] = static_cast<aetheria::world::FactionId>(2);
    auto live_site = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, test_ruleset());
    aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, live_site, test_ruleset());

    aetheria::zone::Zone region{
        aetheria::zone::child_key(aetheria::zone::kRootZone, kReductionRegionId, 0)};
    const auto placeholder = *region.reg.view<aetheria::zone::ZoneMeta>().begin();
    constexpr aetheria::time::Tick event_tick{1234};
    region.reg.emplace<aetheria::world::TurnClock>(placeholder, event_tick);
    auto& region_tiles = std::get<aetheria::zone::RegionPayload>(region.payload)
                             .layers.emplace(0, std::move(tiles)).first->second;
    aetheria::zone::FileZoneStore store{directory.path(), test_ruleset()};
    store.save(aetheria::zone::Zone{aetheria::zone::kRootZone});
    store.save(region);
    store.write_manifest(aetheria::zone::SaveManifest{
        .world_seed = kReductionWorldSeed, .raws_hash = raws_hash});
    const auto before_hash = aetheria::sim::world_state_hash(directory.path());
    auto& layers = std::get<aetheria::zone::SitePayload>(live_site.payload).layers;
    const SiteBuildingStateEvent event{Significance::Region,
                                       layers.persistent.buildings.front().tile,
                                       BuildingState::Idle};

    const bool escalated = aetheria::site::apply_site_building_state_event(
        region_tiles, kReductionCoordinate, live_site, event, test_ruleset());
    store.save(region);
    const auto after_hash = aetheria::sim::world_state_hash(directory.path());

    const auto minimum_milliseconds = aetheria::tests::minimum_milliseconds_after_warmup([&] {
        auto performance_tiles = reduction_region();
        auto performance_site = aetheria::site::materialize_site_zone(
            performance_tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
            test_ruleset());
        auto& performance_layers =
            std::get<aetheria::zone::SitePayload>(performance_site.payload).layers;
        const SiteBuildingStateEvent performance_event{
            Significance::Region, performance_layers.persistent.buildings.front().tile,
            BuildingState::Idle};
        const auto start = std::chrono::steady_clock::now();
        const bool performance_escalated = aetheria::site::apply_site_building_state_event(
            performance_tiles, kReductionCoordinate, performance_site, performance_event,
            test_ruleset());
        const auto elapsed = std::chrono::steady_clock::now() - start;
        EXPECT_TRUE(performance_escalated);
        return std::chrono::duration<double, std::milli>{elapsed}.count();
    });

    EXPECT_TRUE(escalated);
    EXPECT_EQ(region_tiles.reduction_value<PopulationReduction>(kReductionCoordinate), 75U);
    EXPECT_EQ(region_tiles.reduction_value<DevelopmentLevelReduction>(kReductionCoordinate), 1U);
    EXPECT_EQ(region.reg.get<aetheria::world::TurnClock>(placeholder).now, event_tick);
    EXPECT_EQ(before_hash.zone_count, 2U);
    EXPECT_EQ(after_hash.zone_count, 2U);
    EXPECT_NE(before_hash.hash, after_hash.hash);
    EXPECT_LT(minimum_milliseconds, 30.0);
#ifdef NDEBUG
    constexpr auto build_kind = "Release";
#else
    constexpr auto build_kind = "Debug";
#endif
    std::cout << "event_immediate significance=Region tick_before=1234 tick_after=1234 "
                 "before_population=100 after_population=75 region_turns=0 before_hash="
              << before_hash.hash << " after_hash=" << after_hash.hash
              << ' ' << build_kind << "_min_of_5_ms=" << minimum_milliseconds << '\n';
}

}  // namespace

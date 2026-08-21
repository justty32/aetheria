#include "core/serialize/normalized_state_hash.h"
#include "core/serialize/zone_codec.h"
#include "core/site/site_materialize.h"
#include "core/site/site_reduction.h"
#include "tests/site/site_build_loop_test_support.h"

#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::site::CityBuildState;
using aetheria::tests::build_fixture;
using aetheria::tests::kBuildCoordinate;
using aetheria::tests::kBuildRegionId;
using aetheria::tests::kBuildSiteKey;
using aetheria::tests::queue_layout;
using aetheria::tests::test_ruleset;
using aetheria::world::ProductionStockReduction;
using aetheria::zone::InMemoryZoneStore;
using aetheria::zone::Zone;

TEST(SiteBuildLoop, AbsoluteXunReductionCannotCountHourlyProductionTwiceAndStatePersists) {
    auto fixture = build_fixture();
    queue_layout(fixture.site, true);
    const auto hash_before =
        aetheria::serialize::normalized_state_hash(fixture.site, test_ruleset());
    InMemoryZoneStore store{test_ruleset()};
    aetheria::site::SiteTurnPipeline pipeline{test_ruleset(), store};
    static_cast<void>(
        pipeline.advance_hours(fixture.site, fixture.region, 0, kBuildCoordinate, 240));
    auto& tiles = std::get<aetheria::zone::RegionPayload>(fixture.region.payload).layers.at(0);
    const auto once = tiles.reduction_value<ProductionStockReduction>(kBuildCoordinate);
    aetheria::site::reduce_live_site_xun(tiles, kBuildCoordinate, fixture.site);
    aetheria::site::reduce_live_site_xun(tiles, kBuildCoordinate, fixture.site);
    const auto thrice = tiles.reduction_value<ProductionStockReduction>(kBuildCoordinate);
    EXPECT_EQ(once, thrice);
    EXPECT_EQ(thrice, aetheria::site::city_build_state(fixture.site).economy.production_stock);
    EXPECT_NE(hash_before,
              aetheria::serialize::normalized_state_hash(fixture.site, test_ruleset()));

    const auto bytes = aetheria::serialize::encode_zone(fixture.site, test_ruleset());
    const auto loaded = aetheria::serialize::decode_zone(bytes, test_ruleset());
    const auto loaded_states = loaded->reg.view<const CityBuildState>();
    ASSERT_EQ(loaded_states.size(), 1U);
    EXPECT_EQ(loaded_states.get<const CityBuildState>(*loaded_states.begin()),
              aetheria::site::city_build_state(fixture.site));
    std::cout << "site_no_double_count hourly_production=" << once
              << " after_two_extra_reductions=" << thrice
              << " persistent_city_build_state=1 format_v="
              << aetheria::serialize::kSaveFormatVersion << '\n';
}

TEST(SiteBuildLoop, PendingConstructionSurvivesColdRematerializeWithRemainingHours) {
    auto fixture = build_fixture();
    aetheria::site::start_construction(fixture.site, "city.house", {10, 10}, test_ruleset());
    aetheria::site::start_construction(fixture.site, "city.farm", {20, 20}, test_ruleset());
    InMemoryZoneStore store{test_ruleset()};
    aetheria::site::SiteTurnPipeline pipeline{test_ruleset(), store};
    const auto report =
        pipeline.advance_hours(fixture.site, fixture.region, 0, kBuildCoordinate, 36);
    ASSERT_EQ(report.constructions_completed, 1U);
    EXPECT_EQ(aetheria::world::turn_clock(fixture.region).now,
              aetheria::time::Tick{} + aetheria::time::kHour * 36);
    store.save(fixture.site);

    auto& tiles = std::get<aetheria::zone::RegionPayload>(fixture.region.payload).layers.at(0);
    tiles.site[0].has_live_site = false;
    tiles.site[0].lod = aetheria::zone::LodLevel::Absent;
    aetheria::zone::ZoneManager manager{store};
    ASSERT_FALSE(manager.get(kBuildSiteKey).has_value());
    const auto handle = aetheria::site::rematerialize_site_zone(
        manager, tiles, kBuildCoordinate, UINT64_C(0xA37E1222), kBuildRegionId,
        test_ruleset());
    ASSERT_TRUE(manager.with(handle, [](const Zone& loaded) {
        const auto& state = aetheria::site::city_build_state(loaded);
        ASSERT_EQ(state.buildings.size(), 1U);
        ASSERT_EQ(state.pending.size(), 1U);
        EXPECT_EQ(state.pending.front().definition_id, "city.farm");
        EXPECT_EQ(state.pending.front().remaining_hours, 12U);
    }));
    std::cout << "site_pending_cold_roundtrip completed=1 pending=1 remaining_hours=12 "
                 "cold_absent_assertion=1\n";
}

}  // namespace

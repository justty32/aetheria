#include "tests/site/site_unload_equivalence_test_support.h"

namespace {

using namespace aetheria::tests::site_unload_equivalence;

TEST(SiteLifecycle, PopulationFractionSurvivesUnloadReload) {
    LifecycleFixture fixture;
    initialize_lifecycle_fixture(fixture);
    auto& tiles = std::get<aetheria::zone::RegionPayload>(fixture.region.payload).layers.at(0);
    aetheria::zone::InMemoryZoneStore store{test_ruleset()};
    store.save(fixture.site);
    tiles.site[0].has_live_site = false;
    tiles.site[0].lod = aetheria::zone::LodLevel::Absent;
    aetheria::zone::ZoneManager manager{store};
    const auto handle = aetheria::site::rematerialize_site_zone(
        manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
        test_ruleset());
    ASSERT_TRUE(manager.with(handle, [&](aetheria::zone::Zone& site) {
        aetheria::site::enter_full_site(site, tiles, kReductionCoordinate);
    }));
    aetheria::site::SiteTurnPipeline pipeline{test_ruleset(), store};
    std::int64_t before{};
    ASSERT_TRUE(manager.with(handle, [&](aetheria::zone::Zone& site) {
        static_cast<void>(pipeline.advance_hours(
            site, fixture.region, 0, kReductionCoordinate, 1));
        before = aetheria::site::city_build_state(site).economy.population_micro_remainder;
    }));
    ASSERT_NE(before, 0);
    aetheria::site::unload_site_zone(manager, handle, tiles, kReductionCoordinate,
                                     kReductionWorldSeed, kReductionRegionId,
                                     region_now(fixture.region), test_ruleset());
    const auto reloaded = aetheria::site::rematerialize_site_zone(
        manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
        region_now(fixture.region), test_ruleset());
    ASSERT_TRUE(manager.with(reloaded, [&](const aetheria::zone::Zone& site) {
        EXPECT_EQ(aetheria::site::city_build_state(site).economy.population_micro_remainder,
                  before);
    }));
}

TEST(SiteLifecycle, SkippingCatchUpIsDetectedAboveTenPercent) {
    constexpr std::uint32_t xun = 20;
    const auto full = run_full(xun);
    LifecycleFixture fixture;
    initialize_lifecycle_fixture(fixture);
    auto& tiles = std::get<aetheria::zone::RegionPayload>(fixture.region.payload).layers.at(0);
    aetheria::zone::InMemoryZoneStore store{test_ruleset()};
    store.save(fixture.site);
    tiles.site[0].has_live_site = false;
    tiles.site[0].lod = aetheria::zone::LodLevel::Absent;
    aetheria::zone::ZoneManager manager{store};
    const auto handle = aetheria::site::rematerialize_site_zone(
        manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
        test_ruleset());
    ASSERT_TRUE(manager.with(handle, [&](aetheria::zone::Zone& site) {
        aetheria::site::enter_full_site(site, tiles, kReductionCoordinate);
    }));
    aetheria::site::unload_site_zone(manager, handle, tiles, kReductionCoordinate,
                                     kReductionWorldSeed, kReductionRegionId,
                                     aetheria::time::Tick{}, test_ruleset());
    aetheria::world::RegionTurnPipeline region_pipeline{test_ruleset(), store};
    const auto target = aetheria::time::Tick{} +
                        aetheria::time::kXun * static_cast<std::int64_t>(xun);
    advance_absent_to(fixture.region, target, region_pipeline);

    ASSERT_TRUE(manager.load(kSiteKey));
    const auto cold = *manager.get(kSiteKey);
    TileQuantities broken;
    ASSERT_TRUE(manager.with(cold, [&](aetheria::zone::Zone& site) {
        ASSERT_EQ(site.reg.view<const aetheria::site::SiteDigest>().size(), 1U);
        ASSERT_TRUE(site.reg.view<const aetheria::site::CityBuildState>().empty());
        site.lod = aetheria::zone::LodLevel::Coarse;
        tiles.site[0].has_live_site = true;
        aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, site, test_ruleset());
        broken = quantities(tiles);
    }));
    const auto error = maximum_error(full.quantities, broken);
    EXPECT_GT(error, 0.10);
    std::cout << "site_unload_negative_control N=20 full_population="
              << full.quantities.population << " skipped_catch_up_population="
              << broken.population << " max_relative_error=" << error << '\n';
}

TEST(SiteLifecycle, AgingIsClosedFormAndSaturatesAfterTwentyFourXun) {
    LifecycleFixture fixture;
    initialize_lifecycle_fixture(fixture, aetheria::world::FactionId{});
    auto& tiles = std::get<aetheria::zone::RegionPayload>(fixture.region.payload).layers.at(0);
    aetheria::zone::InMemoryZoneStore store{test_ruleset()};
    store.save(fixture.site);
    tiles.site[0].has_live_site = false;
    tiles.site[0].lod = aetheria::zone::LodLevel::Absent;
    aetheria::zone::ZoneManager manager{store};
    const auto handle = aetheria::site::rematerialize_site_zone(
        manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
        test_ruleset());
    ASSERT_TRUE(manager.with(handle, [&](aetheria::zone::Zone& site) {
        aetheria::site::enter_full_site(site, tiles, kReductionCoordinate);
    }));
    aetheria::site::unload_site_zone(manager, handle, tiles, kReductionCoordinate,
                                     kReductionWorldSeed, kReductionRegionId,
                                     aetheria::time::Tick{}, test_ruleset());
    SiteCatchUpReport report;
    constexpr std::uint32_t xun = 10'000;
    const auto now = aetheria::time::Tick{} +
                     aetheria::time::kXun * static_cast<std::int64_t>(xun);
    const auto reloaded = aetheria::site::rematerialize_site_zone(
        manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, now,
        test_ruleset(), &report);
    ASSERT_TRUE(manager.with(reloaded, [](const aetheria::zone::Zone& site) {
        const auto& objects =
            std::get<aetheria::zone::SitePayload>(site.payload).layers.persistent.buildings;
        ASSERT_EQ(objects.size(), 1U);
        EXPECT_EQ(objects.front().state, aetheria::site::BuildingState::Ruined);
        EXPECT_EQ(objects.front().aging_seconds,
                  static_cast<std::uint32_t>(aetheria::site::kBuildingAgingCap));
    }));
    EXPECT_TRUE(report.aging_cap_hit);
    EXPECT_EQ(report.persistent_objects_advanced, 1U);
    EXPECT_EQ(report.aging_transitions, 3U);
    EXPECT_EQ(report.aging_seconds_applied,
              static_cast<std::uint64_t>(aetheria::site::kBuildingAgingCap));
    std::cout << "site_unload_saturation N=" << xun
              << " objects_advanced=" << report.persistent_objects_advanced
              << " aging_transitions=" << report.aging_transitions
              << " cap_hit=" << report.aging_cap_hit << " cap_xun=24 first_clamped_N=25\n";
}

}  // namespace

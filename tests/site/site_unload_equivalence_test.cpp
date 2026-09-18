#include "tests/site/site_unload_equivalence_test_support.h"

namespace {

using namespace aetheria::tests::site_unload_equivalence;

[[nodiscard]] PathResult run_absent(std::uint32_t xun) {
    LifecycleFixture fixture;
    initialize_lifecycle_fixture(fixture);
    auto& tiles = std::get<aetheria::zone::RegionPayload>(fixture.region.payload).layers.at(0);
    const auto slow_before = aetheria::site::split_site_vars(tiles, kReductionCoordinate).slow;
    const auto before_unload_hash = aetheria::site::hash_site_skeleton(
        aetheria::site::build_site_skeleton(
            slow_before,
            aetheria::site::derive_site_seed(kReductionWorldSeed, kReductionRegionId, 0, 0),
            test_ruleset()));
    EXPECT_EQ(before_unload_hash,
              aetheria::site::hash_site_skeleton(
                  std::get<aetheria::zone::SitePayload>(fixture.site.payload)
                      .layers.procedural.skeleton));
    aetheria::zone::InMemoryZoneStore store{test_ruleset()};
    store.save(fixture.site);
    tiles.site[0].has_live_site = false;
    tiles.site[0].lod = aetheria::zone::LodLevel::Absent;
    aetheria::zone::ZoneManager manager{store};
    const auto handle = aetheria::site::rematerialize_site_zone(
        manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
        test_ruleset());
    const bool entered = manager.with(handle, [&](aetheria::zone::Zone& site) {
        aetheria::site::enter_full_site(site, tiles, kReductionCoordinate);
    });
    if (!entered) {
        throw std::runtime_error{"卸載前 Site 未留在 manager"};
    }
    aetheria::site::unload_site_zone(manager, handle, tiles, kReductionCoordinate,
                                     kReductionWorldSeed, kReductionRegionId,
                                     aetheria::time::Tick{}, test_ruleset());
    EXPECT_FALSE(manager.get(kSiteKey).has_value());

    aetheria::world::RegionTurnPipeline region_pipeline{test_ruleset(), store};
    const auto elapsed = aetheria::time::kXun * static_cast<std::int64_t>(xun);
    const auto target = aetheria::time::Tick{} + elapsed;
    advance_absent_to(fixture.region, target, region_pipeline);
    const auto slow_after = aetheria::site::split_site_vars(tiles, kReductionCoordinate).slow;
    EXPECT_EQ(slow_before, slow_after);
    const auto before_reload_hash = aetheria::site::hash_site_skeleton(
        aetheria::site::build_site_skeleton(
            aetheria::site::split_site_vars(tiles, kReductionCoordinate).slow,
            aetheria::site::derive_site_seed(kReductionWorldSeed, kReductionRegionId, 0, 0),
            test_ruleset()));
    EXPECT_EQ(before_unload_hash, before_reload_hash);
    SiteCatchUpReport report;
    const auto reloaded = aetheria::site::rematerialize_site_zone(
        manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, target,
        test_ruleset(), &report);
    PathResult result;
    const bool borrowed = manager.with(reloaded, [&](const aetheria::zone::Zone& site) {
        aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, site, test_ruleset());
        const auto& layers = std::get<aetheria::zone::SitePayload>(site.payload).layers;
        const auto& state = aetheria::site::city_build_state(site);
        result = {quantities(tiles), layers.persistent.buildings, state.buildings, state.pending,
                  report};
    });
    if (!borrowed) {
        throw std::runtime_error{"重載 Site 未留在 manager"};
    }
    return result;
}

TEST(SiteLifecycle, FullAndAbsentClockPathsStayWithinTenPercentAndKeepPersistentObjects) {
    constexpr std::array<std::uint32_t, 4> samples{1, 5, 20, 100};
    for (const auto xun : samples) {
        const auto full = run_full(xun);
        const auto absent = run_absent(xun);
        const auto error = maximum_error(full.quantities, absent.quantities);
        EXPECT_LT(error, 0.10) << "N=" << xun;
        EXPECT_EQ(full.objects, absent.objects) << "N=" << xun;
        EXPECT_EQ(full.city_buildings, absent.city_buildings) << "N=" << xun;
        EXPECT_EQ(full.pending, absent.pending) << "N=" << xun;
        EXPECT_EQ(absent.catch_up.pending_advanced, 1U);
        EXPECT_EQ(absent.catch_up.constructions_completed, 1U);
        std::cout << "site_unload_equivalence N=" << xun
                  << " full(pop/dev/food/prod)=" << full.quantities.population << '/'
                  << full.quantities.development << '/' << full.quantities.food << '/'
                  << full.quantities.production << " absent(pop/dev/food/prod)="
                  << absent.quantities.population << '/' << absent.quantities.development << '/'
                  << absent.quantities.food << '/' << absent.quantities.production
                  << " max_relative_error=" << error << " pending_advanced="
                  << absent.catch_up.pending_advanced << " completed="
                  << absent.catch_up.constructions_completed << '\n';
    }
}

}  // namespace

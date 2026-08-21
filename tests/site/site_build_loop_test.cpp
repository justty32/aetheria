#include "core/site/site_build_loop.h"
#include "core/world/region_simulation.h"
#include "tests/site/site_build_loop_test_support.h"
#include "tests/support/performance.h"

#include <chrono>
#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::build_fixture;
using aetheria::tests::kBuildCoordinate;
using aetheria::tests::production_at;
using aetheria::tests::queue_layout;
using aetheria::tests::test_ruleset;
using aetheria::zone::InMemoryZoneStore;

TEST(SiteBuildLoop, PopulationChangesAcrossOneFiveAndTwentyXunWithBranchEvidence) {
    auto fixture = build_fixture();
    queue_layout(fixture.site, true);
    const auto skeleton_before = aetheria::site::hash_site_skeleton(
        std::get<aetheria::zone::SitePayload>(fixture.site.payload).layers.procedural.skeleton);
    InMemoryZoneStore store{test_ruleset()};
    aetheria::site::SiteTurnPipeline pipeline{test_ruleset(), store};
    const auto first =
        pipeline.advance_hours(fixture.site, fixture.region, 0, kBuildCoordinate, 240);
    const auto population_1 = aetheria::site::city_build_state(fixture.site).economy.population;
    const auto fifth =
        pipeline.advance_hours(fixture.site, fixture.region, 0, kBuildCoordinate, 960);
    const auto population_5 = aetheria::site::city_build_state(fixture.site).economy.population;
    const auto twentieth =
        pipeline.advance_hours(fixture.site, fixture.region, 0, kBuildCoordinate, 3'600);
    const auto population_20 = aetheria::site::city_build_state(fixture.site).economy.population;
    const auto skeleton_after = aetheria::site::hash_site_skeleton(
        std::get<aetheria::zone::SitePayload>(fixture.site.payload).layers.procedural.skeleton);
    const auto& state = aetheria::site::city_build_state(fixture.site);
    const auto minimum_milliseconds = aetheria::tests::minimum_milliseconds_after_warmup([&] {
        auto measured = build_fixture();
        queue_layout(measured.site, true);
        InMemoryZoneStore measured_store{test_ruleset()};
        aetheria::site::SiteTurnPipeline measured_pipeline{test_ruleset(), measured_store};
        const auto start = std::chrono::steady_clock::now();
        const auto report = measured_pipeline.advance_hours(
            measured.site, measured.region, 0, kBuildCoordinate, 4'800);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        EXPECT_EQ(report.xun_boundaries, 20U);
        return std::chrono::duration<double, std::milli>{elapsed}.count();
    });

    EXPECT_GT(population_1, 100U);
    EXPECT_GT(population_5, population_1);
    EXPECT_GT(population_20, population_5);
    EXPECT_EQ(first.constructions_completed, 5U);
    EXPECT_EQ(first.completion_reductions, 3U);
    EXPECT_EQ(first.xun_boundaries, 1U);
    EXPECT_GT(first.adjacency_bonus_triggers, 0U);
    EXPECT_EQ(fifth.xun_boundaries, 4U);
    EXPECT_EQ(twentieth.xun_boundaries, 15U);
    EXPECT_EQ(skeleton_before, skeleton_after);
    EXPECT_EQ(production_at(fixture), state.economy.production_stock);

#ifdef NDEBUG
    constexpr auto build_kind = "Release";
#else
    constexpr auto build_kind = "Debug";
#endif
    std::cout << "site_population_xun N=0/1/5/20 values=100/" << population_1 << '/'
              << population_5 << '/' << population_20 << '\n'
              << "site_build_branches completed=" << first.constructions_completed
              << " completion_reductions=" << first.completion_reductions
              << " adjacency_triggers_1xun=" << first.adjacency_bonus_triggers
              << " xun_boundaries_total="
              << first.xun_boundaries + fifth.xun_boundaries + twentieth.xun_boundaries
              << " food_produced_total="
              << first.food_produced + fifth.food_produced + twentieth.food_produced
              << " production_stock=" << state.economy.production_stock << '\n'
              << "site_build_loop_" << build_kind << "_20xun_min_of_5_ms="
              << minimum_milliseconds
              << " skeleton_hash=" << skeleton_before << '\n';
}

TEST(SiteBuildLoop, SameBuildingsDifferentPlacementChangesProductionAndSatisfaction) {
    auto good = build_fixture();
    auto bad = build_fixture();
    queue_layout(good.site, true);
    queue_layout(bad.site, false);
    InMemoryZoneStore good_store{test_ruleset()};
    InMemoryZoneStore bad_store{test_ruleset()};
    aetheria::site::SiteTurnPipeline good_pipeline{test_ruleset(), good_store};
    aetheria::site::SiteTurnPipeline bad_pipeline{test_ruleset(), bad_store};
    const auto good_report =
        good_pipeline.advance_hours(good.site, good.region, 0, kBuildCoordinate, 240);
    const auto bad_report =
        bad_pipeline.advance_hours(bad.site, bad.region, 0, kBuildCoordinate, 240);
    const auto& good_state = aetheria::site::city_build_state(good.site);
    const auto& bad_state = aetheria::site::city_build_state(bad.site);

    ASSERT_EQ(good_state.buildings.size(), bad_state.buildings.size());
    EXPECT_GT(good_state.economy.production_stock, bad_state.economy.production_stock);
    EXPECT_GT(good_state.economy.satisfaction, bad_state.economy.satisfaction);
    EXPECT_GT(good_report.adjacency_bonus_triggers, bad_report.adjacency_bonus_triggers);
    std::cout << "site_adjacency_good production=" << good_state.economy.production_stock
              << " satisfaction=" << static_cast<unsigned>(good_state.economy.satisfaction)
              << " triggers=" << good_report.adjacency_bonus_triggers << '\n'
              << "site_adjacency_bad production=" << bad_state.economy.production_stock
              << " satisfaction=" << static_cast<unsigned>(bad_state.economy.satisfaction)
              << " triggers=" << bad_report.adjacency_bonus_triggers << '\n';
}

TEST(SiteBuildLoop, AbsentRegionApproximationAlsoEvolvesPopulation) {
    aetheria::world::RegionTiles tiles{1, 1};
    tiles.settlement[0] = aetheria::world::SettlementTier::Town;
    static_cast<void>(aetheria::world::RegionSimulation::advance_xun(tiles));
    const auto population_1 =
        tiles.reduction_value<aetheria::world::PopulationReduction>({0, 0});
    for (std::size_t xun = 1; xun < 5; ++xun) {
        static_cast<void>(aetheria::world::RegionSimulation::advance_xun(tiles));
    }
    const auto population_5 =
        tiles.reduction_value<aetheria::world::PopulationReduction>({0, 0});
    for (std::size_t xun = 5; xun < 20; ++xun) {
        static_cast<void>(aetheria::world::RegionSimulation::advance_xun(tiles));
    }
    const auto population_20 =
        tiles.reduction_value<aetheria::world::PopulationReduction>({0, 0});
    EXPECT_GT(population_5, population_1);
    EXPECT_GT(population_20, population_5);
    std::cout << "region_absent_population_xun N=1/5/20 values=" << population_1 << '/'
              << population_5 << '/' << population_20 << '\n';
}

}  // namespace

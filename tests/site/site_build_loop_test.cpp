#include "core/serialize/normalized_state_hash.h"
#include "core/site/site_build_loop.h"
#include "core/world/region_simulation.h"
#include "tests/site/site_build_loop_test_support.h"
#include "tests/support/performance.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::build_batch_fixture;
using aetheria::tests::build_fixture;
using aetheria::tests::kAbsentBuildCoordinate;
using aetheria::tests::kBuildCoordinate;
using aetheria::tests::kSecondBuildCoordinate;
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

TEST(SiteBuildLoop, BatchNormalizesOrderAndSettlesRegionOnceWithDetectableControl) {
    auto forward = build_batch_fixture();
    auto reverse = build_batch_fixture();
    InMemoryZoneStore forward_store{test_ruleset()};
    InMemoryZoneStore reverse_store{test_ruleset()};
    aetheria::site::SiteTurnPipeline forward_pipeline{test_ruleset(), forward_store};
    aetheria::site::SiteTurnPipeline reverse_pipeline{test_ruleset(), reverse_store};
    const std::array forward_targets{
        aetheria::site::SiteAdvanceTarget{&forward.first, 0, kBuildCoordinate},
        aetheria::site::SiteAdvanceTarget{&forward.second, 0, kSecondBuildCoordinate},
    };
    const std::array reverse_targets{
        aetheria::site::SiteAdvanceTarget{&reverse.second, 0, kSecondBuildCoordinate},
        aetheria::site::SiteAdvanceTarget{&reverse.first, 0, kBuildCoordinate},
    };
    const auto forward_report =
        forward_pipeline.advance_hours(forward.region, forward_targets, 240);
    const auto reverse_report =
        reverse_pipeline.advance_hours(reverse.region, reverse_targets, 240);
    const auto& forward_tiles =
        std::get<aetheria::zone::RegionPayload>(forward.region.payload).layers.at(0);
    const auto& reverse_tiles =
        std::get<aetheria::zone::RegionPayload>(reverse.region.payload).layers.at(0);
    const auto batch_population =
        forward_tiles.reduction_value<aetheria::world::PopulationReduction>(kAbsentBuildCoordinate);

    EXPECT_EQ(forward_report.region_xun_advances, 1U);
    ASSERT_EQ(reverse_report.sites.size(), 2U);
    EXPECT_LT(aetheria::zone::value_of(reverse_report.sites[0].site_key),
              aetheria::zone::value_of(reverse_report.sites[1].site_key));
    EXPECT_EQ(reverse_report.site_hours_advanced, 480U);
    EXPECT_EQ(reverse_report.site_xun_boundaries, 2U);
    EXPECT_EQ(reverse_report.region_xun_advances, 1U);
    EXPECT_EQ(reverse_report.reduction_writes, 2U);
    EXPECT_EQ(reverse_report.xun_reduction_writes, 2U);
    const auto first_population =
        aetheria::site::city_build_state(forward.first).economy.population;
    const auto second_population =
        aetheria::site::city_build_state(forward.second).economy.population;
    EXPECT_EQ(forward_tiles.reduction_value<aetheria::world::PopulationReduction>(
                  kBuildCoordinate),
              first_population);
    EXPECT_EQ(forward_tiles.reduction_value<aetheria::world::PopulationReduction>(
                  kSecondBuildCoordinate),
              second_population);
    EXPECT_EQ(
        reverse_tiles.reduction_value<aetheria::world::PopulationReduction>(kAbsentBuildCoordinate),
        batch_population);
    EXPECT_EQ(aetheria::serialize::normalized_state_hash(forward.region, test_ruleset()),
              aetheria::serialize::normalized_state_hash(reverse.region, test_ruleset()));
    EXPECT_EQ(aetheria::serialize::normalized_state_hash(forward.first, test_ruleset()),
              aetheria::serialize::normalized_state_hash(reverse.first, test_ruleset()));
    EXPECT_EQ(aetheria::serialize::normalized_state_hash(forward.second, test_ruleset()),
              aetheria::serialize::normalized_state_hash(reverse.second, test_ruleset()));
    EXPECT_EQ(forward_tiles.site[0].lod, aetheria::zone::LodLevel::Full);
    EXPECT_EQ(forward_tiles.site[1].lod, aetheria::zone::LodLevel::Full);

    auto old_shape = build_batch_fixture();
    InMemoryZoneStore old_store{test_ruleset()};
    aetheria::site::SiteTurnPipeline old_pipeline{test_ruleset(), old_store};
    static_cast<void>(
        old_pipeline.advance_hours(old_shape.first, old_shape.region, 0, kBuildCoordinate, 240));
    static_cast<void>(old_pipeline.advance_hours(old_shape.second, old_shape.region, 0,
                                                 kSecondBuildCoordinate, 240));
    const auto& old_tiles =
        std::get<aetheria::zone::RegionPayload>(old_shape.region.payload).layers.at(0);
    const auto double_population =
        old_tiles.reduction_value<aetheria::world::PopulationReduction>(kAbsentBuildCoordinate);
    const auto once =
        aetheria::world::region_formula(aetheria::world::SettlementTier::Town, 0, 0, 0);
    const auto twice =
        aetheria::world::region_formula(aetheria::world::SettlementTier::Town, once.population,
                                        once.food_stock, once.production_stock);
    EXPECT_EQ(batch_population, once.population);
    EXPECT_EQ(double_population, twice.population);
    EXPECT_NE(double_population, batch_population);

    std::cout << "site_batch sites=" << reverse_report.sites.size()
              << " site_hours=" << reverse_report.site_hours_advanced
              << " site_xun_boundaries=" << reverse_report.site_xun_boundaries
              << " region_xun_advances=" << reverse_report.region_xun_advances
              << " reduction_writes=" << reverse_report.reduction_writes
              << " xun_reduction_writes=" << reverse_report.xun_reduction_writes
              << " live_populations=" << first_population << '/' << second_population
              << " absent_population_once=" << batch_population
              << " old_sequential_population_twice=" << double_population << '\n';
}

TEST(SiteBuildLoop, SingleWrapperMatchesOneElementBatchFieldByField) {
    auto wrapper = build_fixture();
    auto batch = build_fixture();
    queue_layout(wrapper.site, true);
    queue_layout(batch.site, true);
    InMemoryZoneStore wrapper_store{test_ruleset()};
    InMemoryZoneStore batch_store{test_ruleset()};
    aetheria::site::SiteTurnPipeline wrapper_pipeline{test_ruleset(), wrapper_store};
    aetheria::site::SiteTurnPipeline batch_pipeline{test_ruleset(), batch_store};
    const auto wrapper_report =
        wrapper_pipeline.advance_hours(wrapper.site, wrapper.region, 0, kBuildCoordinate, 240);
    const std::array target{
        aetheria::site::SiteAdvanceTarget{&batch.site, 0, kBuildCoordinate},
    };
    const auto batch_report = batch_pipeline.advance_hours(batch.region, target, 240);

    ASSERT_EQ(batch_report.sites.size(), 1U);
    EXPECT_EQ(wrapper_report, batch_report.sites.front().report);
    EXPECT_EQ(batch_report.site_hours_advanced, 240U);
    EXPECT_EQ(batch_report.site_xun_boundaries, 1U);
    EXPECT_EQ(batch_report.region_xun_advances, 1U);
    EXPECT_EQ(aetheria::serialize::normalized_state_hash(wrapper.region, test_ruleset()),
              aetheria::serialize::normalized_state_hash(batch.region, test_ruleset()));
    EXPECT_EQ(aetheria::serialize::normalized_state_hash(wrapper.site, test_ruleset()),
              aetheria::serialize::normalized_state_hash(batch.site, test_ruleset()));
}

TEST(SiteBuildLoop, SiteJoiningMidXunUsesTheSingleRegionClockBoundary) {
    auto fixture = build_batch_fixture();
    InMemoryZoneStore store{test_ruleset()};
    aetheria::site::SiteTurnPipeline pipeline{test_ruleset(), store};
    static_cast<void>(pipeline.advance_hours(fixture.first, fixture.region, 0,
                                             kBuildCoordinate, 36));
    EXPECT_EQ(aetheria::world::turn_clock(fixture.region).now,
              aetheria::time::Tick{} + aetheria::time::kHour * 36);

    const std::array targets{
        aetheria::site::SiteAdvanceTarget{&fixture.first, 0, kBuildCoordinate},
        aetheria::site::SiteAdvanceTarget{&fixture.second, 0, kSecondBuildCoordinate},
    };
    const auto report = pipeline.advance_hours(fixture.region, targets, 204);

    EXPECT_EQ(report.sites.size(), 2U);
    EXPECT_EQ(report.site_xun_boundaries, 2U);
    EXPECT_EQ(report.region_xun_advances, 1U);
    EXPECT_EQ(aetheria::world::turn_clock(fixture.region).now,
              aetheria::time::Tick{} + aetheria::time::kXun);
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

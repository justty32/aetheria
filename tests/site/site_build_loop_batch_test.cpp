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

}  // namespace

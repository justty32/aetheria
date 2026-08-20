#include "core/worldgen/gen_stage_ids.h"
#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/worldgen/history_identity_test_support.h"

#include <algorithm>
#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::history_identity::IdentityMetrics;
using aetheria::tests::history_identity::kIdentityBlock;
using aetheria::tests::history_identity::kSharedWeightBlock;
using aetheria::tests::history_identity::measure_identity;
using aetheria::tests::history_identity::measure_roads;
using aetheria::tests::history_identity::reuse_percent;
using aetheria::tests::history_identity::RoadMetrics;
using aetheria::tests::history_identity::ruleset_replacing;
using aetheria::tests::test_ruleset;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::generate_cities;
using aetheria::worldgen::RegionSlowVariables;

TEST(HistoryIdentity, ReportsSelectionFeedbackAndAncientRoadProbesAcrossEightRegions) {
    constexpr auto seed = UINT64_C(20260820);
    const auto zero_bonus =
        ruleset_replacing("ancient_site_bonus = 10000", "ancient_site_bonus = 0");
    const auto full_cost = ruleset_replacing("ancient_road_reuse_numerator = 1\n"
                                             "ancient_road_reuse_denominator = 2",
                                             "ancient_road_reuse_numerator = 1\n"
                                             "ancient_road_reuse_denominator = 1");
    const auto shared_weights = ruleset_replacing(kIdentityBlock, kSharedWeightBlock);
    IdentityMetrics split_identity;
    IdentityMetrics shared_weight_identity;
    std::size_t zero_bonus_overlap{};
    RoadMetrics half_cost_roads;
    RoadMetrics full_cost_roads;
    for (std::uint32_t region_id = 0; region_id < 8U; ++region_id) {
        const RegionSlowVariables slow{region_id, 128, 96};
        const auto split = build_skeleton(slow, seed, test_ruleset());
        const auto shared = build_skeleton(slow, seed, shared_weights);
        const auto no_discount = build_skeleton(slow, seed, full_cost);
        const auto zero_cities = generate_cities(
            split.skeleton.elevation, split.climate, split.rivers, split.biome, split.history,
            zero_bonus,
            aetheria::worldgen::derive_region_stage_seed(
                seed, region_id, aetheria::worldgen::detail::kCityStageId),
            {});
        split_identity += measure_identity(split);
        shared_weight_identity += measure_identity(shared);
        zero_bonus_overlap += static_cast<std::size_t>(std::ranges::count_if(
            zero_cities.cities,
            [&](const auto& city) { return split.history.survivor[city.canonical_id] != 0; }));
        half_cost_roads += measure_roads(split, test_ruleset());
        full_cost_roads += measure_roads(no_discount, full_cost);
    }

    EXPECT_EQ(split_identity.ancient_sites, 96U);
    EXPECT_EQ(split_identity.survivors, 24U);
    EXPECT_GT(split_identity.survivor_overlap, zero_bonus_overlap);
    EXPECT_GT(half_cost_roads.ancient_edges, 0U);
    EXPECT_LE(half_cost_roads.unoverwritten_edges, half_cost_roads.ancient_edges);
    ASSERT_GT(half_cost_roads.eligible, 0U);
    ASSERT_GT(full_cost_roads.eligible, 0U);

    std::cout << "history_identity shared_weight_exact_site_overlap="
              << shared_weight_identity.exact_site_overlap << '/'
              << shared_weight_identity.ancient_sites << " split_exact_site_overlap="
              << split_identity.exact_site_overlap << '/' << split_identity.ancient_sites << '\n'
              << "history_feedback enabled_survivor_overlap="
              << split_identity.survivor_overlap << " zero_bonus_survivor_overlap="
              << zero_bonus_overlap << " survivors=" << split_identity.survivors << '\n'
              << "ancient_road_wilderness total=" << half_cost_roads.ancient_edges
              << " unoverwritten=" << half_cost_roads.unoverwritten_edges << '\n'
              << "ancient_road_reuse_half reused=" << half_cost_roads.reused
              << " eligible=" << half_cost_roads.eligible
              << " percent=" << reuse_percent(half_cost_roads) << '\n'
              << "ancient_road_reuse_full_cost reused=" << full_cost_roads.reused
              << " eligible=" << full_cost_roads.eligible
              << " percent=" << reuse_percent(full_cost_roads) << '\n';
}

}  // namespace

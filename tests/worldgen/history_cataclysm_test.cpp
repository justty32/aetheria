#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::test_ruleset;
using aetheria::world::SettlementTier;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::CitySite;
using aetheria::worldgen::RegionSlowVariables;

TEST(HistoryCataclysm, ScoreDistributionsOverlapAndSurvivorsLeaveFoundations) {
    const auto result =
        build_skeleton(RegionSlowVariables{7, 128, 96}, UINT64_C(20260820), test_ruleset());
    std::vector<std::int32_t> survivor_scores;
    std::vector<std::int32_t> ruined_scores;
    const CitySite* ruined_city = nullptr;
    for (const auto& site : result.history.ancient_sites.cities) {
        if (result.history.survivor[site.canonical_id] != 0) {
            survivor_scores.push_back(site.score);
            EXPECT_EQ(result.history.features.feature[site.canonical_id],
                      result.skeleton.definitions.ancient_foundation);
            continue;
        }
        ruined_scores.push_back(site.score);
        if (site.tier == SettlementTier::City &&
            (ruined_city == nullptr || site.score > ruined_city->score)) {
            ruined_city = &site;
        }
    }
    std::ranges::sort(survivor_scores);
    std::ranges::sort(ruined_scores);
    ASSERT_FALSE(survivor_scores.empty());
    ASSERT_FALSE(ruined_scores.empty());
    EXPECT_LE(survivor_scores.front(), ruined_scores.back());
    EXPECT_GE(survivor_scores.back(), ruined_scores.front());
    ASSERT_NE(ruined_city, nullptr);

    const auto tier_index = static_cast<std::size_t>(ruined_city->tier) - 1U;
    const auto ruin = test_ruleset().civilization_rules().history.ruin_features[tier_index];
    const auto* ruin_definition = test_ruleset().feature(ruin);
    ASSERT_NE(ruin_definition, nullptr);
    EXPECT_EQ(result.history.features.feature[ruined_city->canonical_id], ruin);

    std::cout << "cataclysm_survivor_scores min=" << survivor_scores.front()
              << " median=" << survivor_scores[survivor_scores.size() / 2U]
              << " max=" << survivor_scores.back() << " count=" << survivor_scores.size() << '\n'
              << "cataclysm_ruined_scores min=" << ruined_scores.front()
              << " median=" << ruined_scores[ruined_scores.size() / 2U]
              << " max=" << ruined_scores.back() << " count=" << ruined_scores.size() << '\n'
              << "high_score_ancient_city_ruined tile=(" << ruined_city->tile.x << ','
              << ruined_city->tile.y << ") score=" << ruined_city->score
              << " original_tier=" << static_cast<unsigned>(ruined_city->tier)
              << " ruin_def=" << ruin_definition->id << '\n';
}

}  // namespace

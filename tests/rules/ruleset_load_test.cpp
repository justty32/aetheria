#include "core/rules/ruleset.h"
#include "tests/support/ruleset_fixture.h"

#include <array>
#include <concepts>
#include <cstdint>
#include <span>
#include <type_traits>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::Ruleset;
using aetheria::rules::TerrainDef;
using aetheria::rules::kEdgeRoadFlag;
using aetheria::rules::kFeatureRuinFlag;
using aetheria::tests::test_ruleset;

static_assert(std::same_as<decltype(std::declval<const Ruleset&>().terrains()),
                           std::span<const TerrainDef>>);
static_assert(!std::is_assignable_v<
              decltype((std::declval<const Ruleset&>().terrains()[0].move_cost)), std::int32_t>);

TEST(RulesetLoader, LoadsFourImmutableDefinitionTypes) {
    const auto& ruleset = test_ruleset();
    ASSERT_EQ(ruleset.terrains().size(), 5U);
    ASSERT_EQ(ruleset.reliefs().size(), 3U);
    ASSERT_EQ(ruleset.features().size(), 8U);
    ASSERT_EQ(ruleset.edges().size(), 18U);
    ASSERT_EQ(ruleset.biome_rules().size(), 6U);
    EXPECT_TRUE(ruleset.biome_rules().back().fallback);
    EXPECT_TRUE(ruleset.movement_rules().loaded);
    EXPECT_TRUE(ruleset.civilization_rules().loaded);
    EXPECT_EQ(ruleset.terrain(*ruleset.find_terrain("terrain.grassland"))->move_cost, 1);
    EXPECT_EQ(ruleset.feature(*ruleset.find_feature("feature.forest"))->required_terrain,
              ruleset.find_terrain("terrain.grassland"));

    const auto& history = ruleset.civilization_rules().history;
    EXPECT_EQ(history.scoring_weights.freshwater, 500);
    EXPECT_EQ(history.scoring_weights.farmland, 6);
    EXPECT_EQ(history.scoring_weights.harbor, 60);
    EXPECT_EQ(history.scoring_weights.defense, 120);
    EXPECT_EQ(history.scoring_weights.resource, 300);
    EXPECT_EQ(history.scoring_weights.bottleneck, 700);
    EXPECT_EQ(history.scoring_weights.extreme_climate_penalty, -260);
    EXPECT_EQ(history.scoring_weights.high_elevation_penalty, -180);
    EXPECT_EQ(history.ancient_site_count, 12U);
    EXPECT_EQ(history.ancient_city_count, 3U);
    EXPECT_EQ(history.ancient_town_count, 4U);
    EXPECT_EQ(history.minimum_spacing, (std::array<std::uint16_t, 3>{5, 8, 12}));
    EXPECT_EQ(history.survivor_percent, 25U);
    EXPECT_EQ(history.ancient_site_bonus, 10000);
    EXPECT_EQ(history.ancient_road_reuse_numerator, 1U);
    EXPECT_EQ(history.ancient_road_reuse_denominator, 2U);
    ASSERT_NE(ruleset.edge(history.road_edge), nullptr);
    EXPECT_EQ(ruleset.edge(history.road_edge)->id, "edge.ancient_road");
    EXPECT_NE(ruleset.edge(history.road_edge)->flags & kEdgeRoadFlag, 0U);
    EXPECT_GT(ruleset.edge(history.road_edge)->move_cost,
              ruleset.edge(*ruleset.find_edge("edge.road"))->move_cost);
    constexpr std::array expected_ruins{"feature.ruin_village", "feature.ruin_town",
                                        "feature.ruin_city"};
    for (std::size_t index = 0; index < expected_ruins.size(); ++index) {
        const auto* ruin = ruleset.feature(history.ruin_features[index]);
        ASSERT_NE(ruin, nullptr);
        EXPECT_EQ(ruin->id, expected_ruins[index]);
        EXPECT_NE(ruin->flags & kFeatureRuinFlag, 0U);
    }
}

}  // namespace

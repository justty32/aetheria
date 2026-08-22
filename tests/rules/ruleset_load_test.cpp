#include <gtest/gtest.h>

#include <array>
#include <concepts>
#include <cstdint>
#include <set>
#include <span>
#include <string>
#include <type_traits>

#include "core/rules/ruleset.h"
#include "tests/support/ruleset_fixture.h"

namespace {

using aetheria::rules::kEdgeRoadFlag;
using aetheria::rules::kFeatureRuinFlag;
using aetheria::rules::Ruleset;
using aetheria::rules::TerrainDef;
using aetheria::tests::test_ruleset;

template <typename Rule>
concept HasMoistureBounds = requires(Rule rule) { rule.min_moisture; };

template <typename Rule>
concept HasTemperatureBounds = requires(Rule rule) { rule.min_temperature_tenths; };

template <typename Rule>
concept HasRuggednessBounds = requires(Rule rule) { rule.min_ruggedness; };

static_assert(
    std::same_as<decltype(std::declval<const Ruleset&>().terrains()), std::span<const TerrainDef>>);
static_assert(!std::is_assignable_v<
              decltype((std::declval<const Ruleset&>().terrains()[0].move_cost)), std::int32_t>);
static_assert(!HasMoistureBounds<aetheria::rules::ReliefRule>);
static_assert(!HasTemperatureBounds<aetheria::rules::ReliefRule>);
static_assert(!HasRuggednessBounds<aetheria::rules::TerrainRule>);

TEST(RulesetLoader, LoadsImmutableDefinitionTypesAndSiteProjectionMapping) {
    const auto& ruleset = test_ruleset();
    ASSERT_FALSE(ruleset.terrains().empty());
    std::set<std::string> terrain_ids;
    for (std::size_t index = 0; index < ruleset.terrains().size(); ++index) {
        const auto& terrain = ruleset.terrains()[index];
        ASSERT_TRUE(terrain_ids.insert(terrain.id).second) << terrain.id;
        const auto found = ruleset.find_terrain(terrain.id);
        ASSERT_TRUE(found.has_value()) << terrain.id;
        EXPECT_EQ(aetheria::rules::value_of(*found), index) << terrain.id;
        EXPECT_EQ(ruleset.terrain(*found), &terrain) << terrain.id;
    }
    ASSERT_EQ(ruleset.reliefs().size(), 3U);
    ASSERT_EQ(ruleset.features().size(), 9U);
    ASSERT_EQ(ruleset.edges().size(), 26U);
    ASSERT_EQ(ruleset.grounds().size(), 6U);
    ASSERT_EQ(ruleset.buildings().size(), 11U);
    ASSERT_EQ(ruleset.city_buildings().size(), 5U);
    ASSERT_EQ(ruleset.furniture().size(), 4U);
    ASSERT_EQ(ruleset.terrain_ground_mappings().size(), ruleset.terrains().size());
    ASSERT_FALSE(ruleset.terrain_rules().empty());
    std::set<aetheria::rules::TerrainId> scored_terrains;
    for (const auto& rule : ruleset.terrain_rules()) {
        ASSERT_TRUE(scored_terrains.insert(rule.terrain).second);
        ASSERT_NE(ruleset.terrain(rule.terrain), nullptr);
    }
    for (std::size_t index = 0; index < ruleset.terrains().size(); ++index) {
        const auto terrain = static_cast<aetheria::rules::TerrainId>(index);
        if ((ruleset.terrains()[index].flags & aetheria::rules::kTerrainWaterFlag) == 0) {
            EXPECT_TRUE(scored_terrains.contains(terrain)) << ruleset.terrains()[index].id;
        }
    }
    ASSERT_EQ(ruleset.relief_rules().size(), 3U);
    EXPECT_TRUE(ruleset.relief_rules().back().fallback);
    EXPECT_TRUE(ruleset.movement_rules().loaded);
    EXPECT_TRUE(ruleset.site_generation_rules().loaded);
    EXPECT_TRUE(ruleset.site_fill_rules().loaded);
    EXPECT_TRUE(ruleset.site_build_rules().loaded);
    EXPECT_TRUE(ruleset.local_building_rules().loaded);
    EXPECT_EQ(
        ruleset.city_building(*ruleset.find_city_building("city.workshop"))->production_per_hour,
        2U);
    EXPECT_EQ(ruleset.city_building(*ruleset.find_city_building("city.workshop"))->adjacency.size(),
              1U);
    ASSERT_EQ(ruleset.site_fill_rules().quotas.size(), 2U);
    EXPECT_EQ(ruleset.site_fill_rules().quotas[0].units_per_block, 250U);
    EXPECT_EQ(ruleset.site_fill_rules().quotas[1].units_per_block, 2U);
    EXPECT_EQ(ruleset.building(*ruleset.find_building("building.cottage"))->frontage, 2U);
    EXPECT_TRUE(ruleset.building(*ruleset.find_building("building.palace"))->landmark);
    const auto* dungeon = ruleset.building(*ruleset.find_building("building.dungeon_entrance"));
    ASSERT_NE(dungeon, nullptr);
    EXPECT_EQ(dungeon->underground, aetheria::rules::UndergroundKind::Dungeon);
    EXPECT_EQ(dungeon->underground_depth, 3U);
    EXPECT_EQ(ruleset.furniture(*ruleset.find_furniture("furniture.bed"))->minimum, 1U);
    EXPECT_NE(ruleset.edge(ruleset.local_building_rules().window_edge)->flags &
                  aetheria::rules::kEdgeWindowFlag,
              0U);
    ASSERT_EQ(ruleset.site_fill_rules().faction_styles.size(), 4U);
    EXPECT_EQ(ruleset.site_fill_rules().faction_styles[1].faction, 1U);
    EXPECT_EQ(ruleset.site_fill_rules().faction_styles[1].landmarks.size(), 2U);
    const auto& fortification = ruleset.site_fill_rules().fortification;
    EXPECT_EQ(fortification.double_wall_defense, 80U);
    EXPECT_NE(ruleset.edge(fortification.gate_edge)->flags & aetheria::rules::kEdgeOpenableFlag,
              0U);
    EXPECT_EQ(ruleset.site_generation_rules().block_split_depth, 5U);
    EXPECT_EQ(ruleset.site_generation_rules().block_cut_min_percent, 36U);
    EXPECT_EQ(ruleset.site_generation_rules().block_cut_max_percent, 44U);
    EXPECT_TRUE(ruleset.civilization_rules().loaded);
    EXPECT_EQ(ruleset.civilization_rules().factions.faction_count, 3U);
    EXPECT_EQ(ruleset.civilization_rules().major_city_count, 6U);
    EXPECT_EQ(ruleset.civilization_rules().bottleneck_barrier_move_cost, 5U);
    EXPECT_EQ(ruleset.civilization_rules().factions.governance_max_cost, 256);
    EXPECT_EQ(ruleset.civilization_rules().factions.influence_season, 1U);
    ASSERT_EQ(ruleset.world_connections().size(), 10U);
    EXPECT_EQ(aetheria::rules::value_of(ruleset.world_connections().front().id), 1U);
    EXPECT_EQ(aetheria::rules::value_of(ruleset.world_connections().back().id), 10U);
    EXPECT_EQ(ruleset.terrain(*ruleset.find_terrain("terrain.grassland"))->move_cost, 1);
    EXPECT_EQ(ruleset.feature(*ruleset.find_feature("feature.forest"))->required_terrain,
              ruleset.find_terrain("terrain.grassland"));
    EXPECT_EQ(ruleset.feature(*ruleset.find_feature("feature.ancient_foundation"))->move_cost, 0);
    const auto grass = *ruleset.find_terrain("terrain.grassland");
    const auto* grass_mapping = ruleset.terrain_ground_mapping(grass);
    ASSERT_NE(grass_mapping, nullptr);
    EXPECT_EQ(ruleset.ground(grass_mapping->ground)->id, "ground.grass");

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

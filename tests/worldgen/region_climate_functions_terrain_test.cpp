#include "core/worldgen/biome_classification.h"
#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/worldgen/worldgen_test_support.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::worldgen::classify_relief;
using aetheria::worldgen::classify_terrain;
using aetheria::worldgen::ClimateStageOutput;
using aetheria::worldgen::generate_climate;
using aetheria::worldgen::generate_rivers;
using aetheria::worldgen::QuantizedElevation;
using aetheria::worldgen::ReliefClassificationInput;
using aetheria::worldgen::RegionSlowVariables;
using aetheria::worldgen::TerrainClassificationInput;

constexpr std::array kBiomeReferenceSeeds{UINT64_C(515151), UINT64_C(12345), UINT64_C(424242)};

struct TerrainHitCounts {
    std::size_t land{};
    std::vector<std::size_t> by_definition;
};

[[nodiscard]] TerrainHitCounts terrain_hits(std::uint64_t seed, std::int16_t latitude,
                                            const aetheria::rules::Ruleset& ruleset) {
    const auto result = aetheria::worldgen::build_skeleton(
        RegionSlowVariables{0, 128, 96, latitude}, seed, ruleset);
    TerrainHitCounts counts{0, std::vector<std::size_t>(ruleset.terrains().size())};
    for (std::size_t index = 0; index < result.skeleton.elevation.land.size(); ++index) {
        if (result.skeleton.elevation.land[index] == 0) {
            continue;
        }
        ++counts.land;
        ++counts.by_definition.at(aetheria::rules::value_of(result.biome.terrain[index]));
    }
    return counts;
}

[[nodiscard]] aetheria::rules::Ruleset two_rule_ruleset(std::string_view first_rule,
                                                        std::string_view second_rule) {
    aetheria::tests::TemporaryDirectory directory;
    aetheria::tests::copy_data_files(directory.path());
    aetheria::tests::write_text(directory.path() / "biomes.toml",
                                std::string{"terrain_defs = []\nterrain_ground = []\n\n"} +
                                    std::string{first_rule} + std::string{second_rule} + R"(
[[relief_rules]]
fallback = true
relief = "relief.plain"
)");
    return aetheria::rules::RulesetLoader::load(directory.path());
}

TEST(RegionGenerationStage, DryMountainKeepsReliefWhileTerrainRemainsDesert) {
    const auto& ruleset = aetheria::tests::test_ruleset();
    const auto terrain = classify_terrain(ruleset, TerrainClassificationInput{170, 1000, 6000});
    const auto relief = classify_relief(ruleset, ReliefClassificationInput{6000, 600});

    ASSERT_NE(ruleset.terrain(terrain), nullptr);
    ASSERT_NE(ruleset.relief(relief), nullptr);
    EXPECT_EQ(ruleset.terrain(terrain)->id, "terrain.desert");
    EXPECT_EQ(ruleset.relief(relief)->id, "relief.mountain");
}

TEST(RegionGenerationStage, TerrainScoreTieUsesDefinitionOrder) {
    constexpr std::string_view grass = R"(
[[terrain_rules]]
temperature_target_tenths = 100
temperature_scale_tenths = 100
terrain = "terrain.grassland"
)";
    constexpr std::string_view desert = R"(
[[terrain_rules]]
temperature_target_tenths = 100
temperature_scale_tenths = 100
terrain = "terrain.desert"
)";
    const auto grass_first = two_rule_ruleset(grass, desert);
    const auto desert_first = two_rule_ruleset(desert, grass);

    const TerrainClassificationInput input{100, 32000, 5000};
    EXPECT_EQ(grass_first.terrain(classify_terrain(grass_first, input))->id,
              "terrain.grassland");
    EXPECT_EQ(desert_first.terrain(classify_terrain(desert_first, input))->id,
              "terrain.desert");
}

TEST(RegionGenerationStage, EveryTerrainRuleHitsAcrossReferenceSeeds) {
    const auto& ruleset = aetheria::tests::test_ruleset();
    std::vector<std::size_t> aggregate(ruleset.terrains().size());
    for (const auto seed : kBiomeReferenceSeeds) {
        const auto counts = terrain_hits(seed, 35, ruleset);
        ASSERT_GT(counts.land, 0U);
        std::size_t above_three_percent{};
        std::size_t largest{};
        for (const auto& rule : ruleset.terrain_rules()) {
            const auto hits = counts.by_definition.at(aetheria::rules::value_of(rule.terrain));
            aggregate.at(aetheria::rules::value_of(rule.terrain)) += hits;
            above_three_percent += hits * 100U > counts.land * 3U;
            largest = std::max(largest, hits);
        }
        EXPECT_GE(above_three_percent, 5U) << "seed=" << seed;
        EXPECT_LT(largest * 2U, counts.land) << "seed=" << seed;
    }
    for (const auto& rule : ruleset.terrain_rules()) {
        const auto* terrain = ruleset.terrain(rule.terrain);
        ASSERT_NE(terrain, nullptr);
        EXPECT_GT(aggregate.at(aetheria::rules::value_of(rule.terrain)), 0U)
            << "死規則未命中：" << terrain->id;
    }
}

TEST(RegionGenerationStage, DeadTerrainRuleProbeNamesFaultInjectedDefinition) {
    constexpr std::string_view dead_tundra = R"(
[[terrain_rules]]
temperature_target_tenths = -32768
temperature_scale_tenths = 1
terrain = "terrain.tundra"
)";
    constexpr std::string_view grass = R"(
[[terrain_rules]]
temperature_target_tenths = 100
temperature_scale_tenths = 100
terrain = "terrain.grassland"
)";
    const auto ruleset = two_rule_ruleset(dead_tundra, grass);
    std::vector<std::size_t> aggregate(ruleset.terrains().size());
    for (const auto seed : kBiomeReferenceSeeds) {
        const auto counts = terrain_hits(seed, 35, ruleset);
        for (std::size_t index = 0; index < aggregate.size(); ++index) {
            aggregate[index] += counts.by_definition[index];
        }
    }
    EXPECT_EQ(aggregate.at(aetheria::rules::value_of(*ruleset.find_terrain("terrain.tundra"))), 0U)
        << "故障注入應讓 terrain.tundra 成為死規則";
    EXPECT_GT(
        aggregate.at(aetheria::rules::value_of(*ruleset.find_terrain("terrain.grassland"))), 0U);
}

TEST(RegionGenerationStage, HighLatitudeIsNotOneHundredPercentOneTerrain) {
    const auto& ruleset = aetheria::tests::test_ruleset();
    const auto counts = terrain_hits(UINT64_C(515151), 70, ruleset);
    const auto active = static_cast<std::size_t>(std::ranges::count_if(
        ruleset.terrain_rules(), [&](const auto& rule) {
            return counts.by_definition.at(aetheria::rules::value_of(rule.terrain)) > 0;
        }));
    std::cout << "high_latitude_active_terrain=" << active;
    for (const auto& rule : ruleset.terrain_rules()) {
        const auto* terrain = ruleset.terrain(rule.terrain);
        ASSERT_NE(terrain, nullptr);
        std::cout << ' ' << terrain->id << '='
                  << counts.by_definition.at(aetheria::rules::value_of(rule.terrain));
    }
    std::cout << '\n';
    EXPECT_GE(active, 2U);
}

}  // namespace

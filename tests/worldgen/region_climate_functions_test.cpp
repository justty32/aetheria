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

TEST(RegionGenerationStage, OnePassClimateProducesAMeasurableRainShadow) {
    const QuantizedElevation elevation{9,
                                       1,
                                       {4090, 4200, 4300, 4400, 6500, 6500, 4400, 4300, 4200},
                                       {0, 1, 1, 1, 1, 1, 1, 1, 1},
                                       4096};
    const auto climate =
        generate_climate(RegionSlowVariables{0, 9, 1, 35}, elevation, UINT64_C(4004), {});

    ASSERT_EQ(climate.prevailing_wind_x, (std::vector<std::int8_t>{100}));
    std::cout << "rain_shadow windward=" << climate.moisture.at(4)
              << " leeward=" << climate.moisture.at(5) << '\n';
    EXPECT_GT(climate.moisture.at(4), climate.moisture.at(5));
    EXPECT_GT(climate.moisture.at(5), 0U);
}

TEST(RegionGenerationStage, WindDirectionAndMoistureBlendAcrossThirtyDegreeBoundary) {
    QuantizedElevation elevation{9, 11, {}, {}, 4096};
    elevation.meters.assign(9U * 11U, 4096);
    elevation.land.assign(9U * 11U, 1);
    for (std::uint32_t y = 0; y < elevation.height; ++y) {
        elevation.land[static_cast<std::size_t>(y) * elevation.width] = 0;
        elevation.land[static_cast<std::size_t>(y) * elevation.width + 8U] = 0;
    }
    const auto climate =
        generate_climate(RegionSlowVariables{0, 9, 11, 30}, elevation, UINT64_C(4004), {});

    ASSERT_EQ(climate.prevailing_wind_x.size(), 11U);
    EXPECT_EQ(climate.prevailing_wind_x[5], 0);
    for (std::size_t y = 1; y < climate.prevailing_wind_x.size(); ++y) {
        EXPECT_LE(std::abs(static_cast<int>(climate.prevailing_wind_x[y]) -
                           static_cast<int>(climate.prevailing_wind_x[y - 1])),
                  40);
    }
    const auto west_coast_land = [](std::size_t y) { return y * 9U + 1U; };
    for (std::size_t y = 3; y <= 7; ++y) {
        EXPECT_LT(climate.moisture[west_coast_land(y - 1)], climate.moisture[west_coast_land(y)]);
    }
}

TEST(RegionGenerationStage, FlatLandMoistureFollowsAirWithoutHittingZero) {
    QuantizedElevation elevation{128, 1, {}, {}, 4096};
    elevation.meters.assign(128, 4200);
    elevation.land.assign(128, 1);
    const auto climate =
        generate_climate(RegionSlowVariables{0, 128, 1, 35}, elevation, UINT64_C(4004), {});

    EXPECT_GT(climate.moisture.front(), climate.moisture.back() * 3U);
    EXPECT_TRUE(std::ranges::all_of(climate.moisture, [](auto value) { return value > 0; }));
}

TEST(RegionGenerationStage, PriorityFloodFillsAClosedDepressionAndTerminates) {
    QuantizedElevation elevation{5, 5, {}, {}, 4096};
    elevation.meters.assign(25, 4000);
    elevation.land.assign(25, 1);
    for (std::uint32_t y = 1; y < 4; ++y) {
        for (std::uint32_t x = 1; x < 4; ++x) {
            elevation.meters[static_cast<std::size_t>(y) * 5U + x] = 5000;
        }
    }
    elevation.meters[12] = 4200;
    ClimateStageOutput climate{5, 5, {}, {}, {}};
    climate.temperature_tenths.assign(25, 100);
    climate.moisture.assign(25, 1000);
    climate.prevailing_wind_x.assign(5, 1);
    const auto rivers = generate_rivers(elevation, climate, UINT64_C(5005), {});

    EXPECT_EQ(rivers.filled_elevation.at(12), 5000);
    auto current = std::size_t{12};
    std::size_t steps{};
    while (rivers.downstream[current] >= 0) {
        current = static_cast<std::size_t>(rivers.downstream[current]);
        ASSERT_LT(++steps, rivers.downstream.size());
    }
    EXPECT_NE(rivers.lake.at(current), 0);
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

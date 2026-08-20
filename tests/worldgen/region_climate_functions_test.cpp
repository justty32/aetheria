#include "core/worldgen/biome_classification.h"
#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
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
    const auto terrain = classify_terrain(ruleset, TerrainClassificationInput{100, 1000, 6000});
    const auto relief = classify_relief(ruleset, ReliefClassificationInput{6000, 600});

    ASSERT_NE(ruleset.terrain(terrain), nullptr);
    ASSERT_NE(ruleset.relief(relief), nullptr);
    EXPECT_EQ(ruleset.terrain(terrain)->id, "terrain.desert");
    EXPECT_EQ(ruleset.relief(relief)->id, "relief.mountain");
}

}  // namespace

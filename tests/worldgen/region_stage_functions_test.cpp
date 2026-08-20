#include "core/rules/ruleset.h"
#include "core/world/region_tiles.h"
#include "core/worldgen/biome_classification.h"
#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::Ruleset;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::classify_relief;
using aetheria::worldgen::classify_terrain;
using aetheria::worldgen::ClimateStageOutput;
using aetheria::worldgen::erode_height;
using aetheria::worldgen::ErosionGenerationConfig;
using aetheria::worldgen::ErosionStageOutput;
using aetheria::worldgen::generate_climate;
using aetheria::worldgen::generate_height;
using aetheria::worldgen::generate_plates;
using aetheria::worldgen::generate_rivers;
using aetheria::worldgen::hash_stage;
using aetheria::worldgen::populate;
using aetheria::worldgen::QuantizedElevation;
using aetheria::worldgen::ReliefClassificationInput;
using aetheria::worldgen::RegionBuildResult;
using aetheria::worldgen::RegionFastVariables;
using aetheria::worldgen::RegionGenerationConfig;
using aetheria::worldgen::RegionSkeleton;
using aetheria::worldgen::RegionSlowVariables;
using aetheria::worldgen::TerrainClassificationInput;

template <typename Input>
concept HasMoistureInput = requires(Input input) { input.moisture; };

template <typename Input>
concept HasTemperatureInput = requires(Input input) { input.temperature_tenths; };

static_assert(
    std::is_invocable_r_v<RegionBuildResult, decltype(&build_skeleton), const RegionSlowVariables&,
                          std::uint64_t, const Ruleset&, const RegionGenerationConfig&>);
static_assert(!std::is_invocable_v<decltype(&build_skeleton), const RegionFastVariables&,
                                   std::uint64_t, const Ruleset&, const RegionGenerationConfig&>);
static_assert(std::is_invocable_r_v<aetheria::world::RegionTiles, decltype(&populate),
                                    const RegionSkeleton&, const RegionFastVariables&>);
static_assert(!std::is_invocable_v<decltype(&populate), const ErosionStageOutput&,
                                   const RegionFastVariables&>);
static_assert(sizeof(aetheria::world::RegionTiles) ==
              aetheria::world::kDeclaredRegionTilesStorageSize);
static_assert(
    std::is_integral_v<typename decltype(ClimateStageOutput::temperature_tenths)::value_type>);
static_assert(
    std::is_integral_v<typename decltype(aetheria::worldgen::RiverStageOutput::flow)::value_type>);
static_assert(!HasMoistureInput<ReliefClassificationInput>);
static_assert(!HasTemperatureInput<ReliefClassificationInput>);

TEST(RegionGenerationStage, PlateStageIsAnIndependentlyRepeatablePureFunction) {
    const RegionSlowVariables slow{17, 128, 96};
    const auto first = generate_plates(slow, UINT64_C(1001), {});
    const auto second = generate_plates(slow, UINT64_C(1001), {});

    EXPECT_EQ(hash_stage(first), hash_stage(second));
    EXPECT_GE(first.plates.size(), 8U);
    EXPECT_LE(first.plates.size(), 16U);
    EXPECT_TRUE(std::any_of(first.boundary_type.begin(), first.boundary_type.end(), [](auto type) {
        return type != aetheria::worldgen::PlateBoundaryType::None;
    }));
}

TEST(RegionGenerationStage, HeightStageIsAnIndependentlyRepeatablePureFunction) {
    const auto plates = generate_plates(RegionSlowVariables{17, 128, 96}, UINT64_C(1001), {});
    const auto first = generate_height(plates, UINT64_C(2002), {});
    const auto second = generate_height(plates, UINT64_C(2002), {});

    EXPECT_EQ(hash_stage(first), hash_stage(second));
    EXPECT_EQ(first.land.size(), 128U * 96U);
}

TEST(RegionGenerationStage, ErosionStageUsesAFixedRepeatableIterationCount) {
    const auto plates = generate_plates(RegionSlowVariables{17, 128, 96}, UINT64_C(1001), {});
    const auto height = generate_height(plates, UINT64_C(2002), {});
    ErosionGenerationConfig twelve_iterations;
    twelve_iterations.iterations = 12;
    auto thirteen_iterations = twelve_iterations;
    thirteen_iterations.iterations = 13;
    const auto first = erode_height(height, UINT64_C(3003), twelve_iterations);
    const auto second = erode_height(height, UINT64_C(3003), twelve_iterations);
    const auto longer = erode_height(height, UINT64_C(3003), thirteen_iterations);

    EXPECT_EQ(hash_stage(first), hash_stage(second));
    EXPECT_NE(hash_stage(first), hash_stage(longer));
}

TEST(RegionGenerationStage, OnePassClimateProducesAMeasurableRainShadow) {
    const QuantizedElevation elevation{9,
                                       1,
                                       {4090, 4200, 4300, 4400, 6500, 6500, 4400, 4300, 4200},
                                       {0, 1, 1, 1, 1, 1, 1, 1, 1},
                                       4096};
    const auto climate =
        generate_climate(RegionSlowVariables{0, 9, 1, 35}, elevation, UINT64_C(4004), {});

    ASSERT_EQ(climate.prevailing_wind_x, (std::vector<std::int8_t>{1}));
    std::cout << "rain_shadow windward=" << climate.moisture.at(4)
              << " leeward=" << climate.moisture.at(5) << '\n';
    EXPECT_GT(climate.moisture.at(4), climate.moisture.at(5));
    EXPECT_GT(climate.moisture.at(5), 0U);
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

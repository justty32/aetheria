#include "core/rules/ruleset.h"
#include "core/world/region_tiles.h"
#include "core/worldgen/biome_classification.h"
#include "core/worldgen/region_generator.h"

#include <algorithm>
#include <cstdint>
#include <type_traits>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::Ruleset;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::ClimateStageOutput;
using aetheria::worldgen::erode_height;
using aetheria::worldgen::ErosionGenerationConfig;
using aetheria::worldgen::ErosionStageOutput;
using aetheria::worldgen::generate_height;
using aetheria::worldgen::generate_plates;
using aetheria::worldgen::hash_stage;
using aetheria::worldgen::populate;
using aetheria::worldgen::ReliefClassificationInput;
using aetheria::worldgen::RegionBuildResult;
using aetheria::worldgen::RegionFastVariables;
using aetheria::worldgen::RegionGenerationConfig;
using aetheria::worldgen::RegionSkeleton;
using aetheria::worldgen::RegionSlowVariables;

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

}  // namespace

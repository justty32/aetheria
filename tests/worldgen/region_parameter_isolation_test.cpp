#include "core/rules/ruleset.h"
#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/worldgen/worldgen_test_support.h"

#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::RulesetLoader;
using aetheria::tests::copy_data_files;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::tests::write_text;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::hash_stage;
using aetheria::worldgen::RegionGenerationConfig;
using aetheria::worldgen::RegionSlowVariables;

TEST(RegionGeneration, ChangingErosionParametersCannotMoveEarlierStageStreams) {
    const RegionSlowVariables slow{23, 128, 96};
    RegionGenerationConfig short_erosion;
    short_erosion.erosion.iterations = 8;
    auto long_erosion = short_erosion;
    long_erosion.erosion.iterations = 16;

    const auto before = build_skeleton(slow, UINT64_C(987654321), test_ruleset(), short_erosion);
    const auto after = build_skeleton(slow, UINT64_C(987654321), test_ruleset(), long_erosion);

    EXPECT_EQ(hash_stage(before.plates), hash_stage(after.plates));
    EXPECT_EQ(hash_stage(before.height), hash_stage(after.height));
    EXPECT_NE(hash_stage(before.erosion), hash_stage(after.erosion));
}

TEST(RegionGeneration, ChangingBiomeParametersCannotMoveStagesOneThroughFive) {
    const RegionSlowVariables slow{23, 128, 96};
    RegionGenerationConfig original;
    auto changed = original;
    changed.biome.moisture_bias = 16000;

    const auto before = build_skeleton(slow, UINT64_C(987654321), test_ruleset(), original);
    const auto after = build_skeleton(slow, UINT64_C(987654321), test_ruleset(), changed);

    std::cout << "stage6_before=" << hash_stage(before.plates) << ',' << hash_stage(before.height)
              << ',' << hash_stage(before.erosion) << ',' << hash_stage(before.climate) << ','
              << hash_stage(before.rivers) << ',' << hash_stage(before.biome)
              << "\nstage6_after=" << hash_stage(after.plates) << ',' << hash_stage(after.height)
              << ',' << hash_stage(after.erosion) << ',' << hash_stage(after.climate) << ','
              << hash_stage(after.rivers) << ',' << hash_stage(after.biome) << '\n';
    EXPECT_EQ(hash_stage(before.plates), hash_stage(after.plates));
    EXPECT_EQ(hash_stage(before.height), hash_stage(after.height));
    EXPECT_EQ(hash_stage(before.erosion), hash_stage(after.erosion));
    EXPECT_EQ(hash_stage(before.climate), hash_stage(after.climate));
    EXPECT_EQ(hash_stage(before.rivers), hash_stage(after.rivers));
    EXPECT_NE(hash_stage(before.biome), hash_stage(after.biome));
}

TEST(RegionGeneration, ChangingClimateParametersCannotMoveStagesOneThroughThree) {
    const RegionSlowVariables slow{23, 128, 96};
    RegionGenerationConfig original;
    auto changed = original;
    changed.climate.air_retention_percent = 98;

    const auto before = build_skeleton(slow, UINT64_C(987654321), test_ruleset(), original);
    const auto after = build_skeleton(slow, UINT64_C(987654321), test_ruleset(), changed);

    std::cout << "stage4_before=" << hash_stage(before.plates) << ',' << hash_stage(before.height)
              << ',' << hash_stage(before.erosion) << ',' << hash_stage(before.climate)
              << "\nstage4_after=" << hash_stage(after.plates) << ','
              << hash_stage(after.height) << ',' << hash_stage(after.erosion) << ','
              << hash_stage(after.climate) << '\n';
    EXPECT_EQ(hash_stage(before.plates), hash_stage(after.plates));
    EXPECT_EQ(hash_stage(before.height), hash_stage(after.height));
    EXPECT_EQ(hash_stage(before.erosion), hash_stage(after.erosion));
    EXPECT_NE(hash_stage(before.climate), hash_stage(after.climate));
}

TEST(RegionGeneration, ChangingFeatureParametersCannotMoveStagesOneThroughSix) {
    const RegionSlowVariables slow{23, 128, 96};
    RegionGenerationConfig original;
    auto changed = original;
    changed.features.forest_density_scale = 1;

    const auto before = build_skeleton(slow, UINT64_C(987654321), test_ruleset(), original);
    const auto after = build_skeleton(slow, UINT64_C(987654321), test_ruleset(), changed);

    std::cout << "stage7_before=" << hash_stage(before.plates) << ',' << hash_stage(before.height)
              << ',' << hash_stage(before.erosion) << ',' << hash_stage(before.climate) << ','
              << hash_stage(before.rivers) << ',' << hash_stage(before.biome) << ','
              << hash_stage(before.features) << "\nstage7_after=" << hash_stage(after.plates) << ','
              << hash_stage(after.height) << ',' << hash_stage(after.erosion) << ','
              << hash_stage(after.climate) << ',' << hash_stage(after.rivers) << ','
              << hash_stage(after.biome) << ',' << hash_stage(after.features) << '\n';
    EXPECT_EQ(hash_stage(before.plates), hash_stage(after.plates));
    EXPECT_EQ(hash_stage(before.height), hash_stage(after.height));
    EXPECT_EQ(hash_stage(before.erosion), hash_stage(after.erosion));
    EXPECT_EQ(hash_stage(before.climate), hash_stage(after.climate));
    EXPECT_EQ(hash_stage(before.rivers), hash_stage(after.rivers));
    EXPECT_EQ(hash_stage(before.biome), hash_stage(after.biome));
    EXPECT_NE(hash_stage(before.features), hash_stage(after.features));
}

TEST(RegionGeneration, EditingBiomeDataChangesOnlyDataDrivenStages) {
    TemporaryDirectory directory;
    copy_data_files(directory.path());
    write_text(directory.path() / "biomes.toml", R"(
terrain_defs = []
terrain_ground = []

[[terrain_rules]]
temperature_target_tenths = 0
temperature_scale_tenths = 1
terrain = "terrain.tundra"

[[relief_rules]]
fallback = true
relief = "relief.plain"
)");
    const auto changed_ruleset = RulesetLoader::load(directory.path());
    const RegionSlowVariables slow{31, 128, 96};
    const auto before = build_skeleton(slow, UINT64_C(6006), test_ruleset());
    const auto after = build_skeleton(slow, UINT64_C(6006), changed_ruleset);

    EXPECT_EQ(hash_stage(before.plates), hash_stage(after.plates));
    EXPECT_EQ(hash_stage(before.height), hash_stage(after.height));
    EXPECT_EQ(hash_stage(before.erosion), hash_stage(after.erosion));
    EXPECT_EQ(hash_stage(before.climate), hash_stage(after.climate));
    EXPECT_EQ(hash_stage(before.rivers), hash_stage(after.rivers));
    EXPECT_NE(hash_stage(before.biome), hash_stage(after.biome));
}

}  // namespace

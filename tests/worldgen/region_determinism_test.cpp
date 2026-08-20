#include "core/rules/ruleset.h"
#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/worldgen/worldgen_test_support.h"

#include <cstdint>
#include <filesystem>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::RulesetLoader;
using aetheria::tests::copy_data_files;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::tests::write_text;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::hash_skeleton;
using aetheria::worldgen::hash_stage;
using aetheria::worldgen::hash_tiles;
using aetheria::worldgen::populate;
using aetheria::worldgen::RegionFastVariables;
using aetheria::worldgen::RegionGenerationConfig;
using aetheria::worldgen::RegionSlowVariables;

TEST(RegionGeneration, SameSeedIsBitIdenticalAcrossEveryStageAndWorldField) {
    const RegionSlowVariables slow{17, 128, 96};
    const auto first = build_skeleton(slow, UINT64_C(12345), test_ruleset());
    const auto second = build_skeleton(slow, UINT64_C(12345), test_ruleset());
    const auto first_tiles = populate(first.skeleton, RegionFastVariables{});
    const auto second_tiles = populate(second.skeleton, RegionFastVariables{});

    EXPECT_EQ(hash_stage(first.plates), hash_stage(second.plates));
    EXPECT_EQ(hash_stage(first.height), hash_stage(second.height));
    EXPECT_EQ(hash_stage(first.erosion), hash_stage(second.erosion));
    EXPECT_EQ(hash_stage(first.climate), hash_stage(second.climate));
    EXPECT_EQ(hash_stage(first.rivers), hash_stage(second.rivers));
    EXPECT_EQ(hash_stage(first.biome), hash_stage(second.biome));
    EXPECT_EQ(hash_stage(first.features), hash_stage(second.features));
    EXPECT_EQ(hash_stage(first.history), hash_stage(second.history));
    EXPECT_EQ(hash_stage(first.cities), hash_stage(second.cities));
    EXPECT_EQ(hash_stage(first.roads), hash_stage(second.roads));
    EXPECT_EQ(hash_stage(first.portals), hash_stage(second.portals));
    EXPECT_EQ(hash_stage(first.factions), hash_stage(second.factions));
    EXPECT_EQ(hash_skeleton(first.skeleton), hash_skeleton(second.skeleton));
    EXPECT_EQ(hash_tiles(first_tiles), hash_tiles(second_tiles));
}

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

TEST(RegionGeneration, EditingBiomeDataChangesOnlyDataDrivenStages) {
    TemporaryDirectory directory;
    copy_data_files(directory.path());
    write_text(directory.path() / "biomes.toml", R"(
[[terrain_rules]]
fallback = true
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

TEST(RegionGeneration, StageAndRegionSeedsAreIndependent) {
    const auto plate = aetheria::worldgen::derive_stage_seed(UINT64_C(77), UINT64_C(1));
    const auto height = aetheria::worldgen::derive_stage_seed(UINT64_C(77), UINT64_C(2));
    const auto region_a = aetheria::worldgen::derive_region_seed(UINT64_C(77), 4);
    const auto region_b = aetheria::worldgen::derive_region_seed(UINT64_C(77), 5);

    EXPECT_NE(plate, height);
    EXPECT_NE(region_a, region_b);
}

TEST(RegionGeneration, ParameterHashesIdentifyTheChangedStageGroup) {
    const RegionGenerationConfig original;
    auto changed = original;
    ++changed.biome.moisture_bias;
    const auto before = aetheria::worldgen::generation_parameter_hashes(original);
    const auto after = aetheria::worldgen::generation_parameter_hashes(changed);

    for (std::size_t index = 0; index < before.groups.size(); ++index) {
        if (index == 5) {
            EXPECT_NE(before.groups[index], after.groups[index]);
        } else {
            EXPECT_EQ(before.groups[index], after.groups[index]);
        }
    }
}

}  // namespace

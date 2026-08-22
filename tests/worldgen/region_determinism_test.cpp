#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"

#include <array>
#include <cstdint>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::test_ruleset;
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

TEST(RegionGeneration, IdentityRedistributionKeepsReferenceWorldHashesBitIdentical) {
    struct ReferenceHash {
        std::uint64_t seed;
        std::uint64_t skeleton;
        std::uint64_t tiles;
    };
    constexpr std::array references{
        ReferenceHash{UINT64_C(515151), UINT64_C(5754128893694281728),
                      UINT64_C(16344487931467028048)},
        ReferenceHash{UINT64_C(12345), UINT64_C(17267498220237237745),
                      UINT64_C(1588818590191442555)},
        ReferenceHash{UINT64_C(424242), UINT64_C(793007085422239155),
                      UINT64_C(15652735773701661944)},
    };

    for (const auto& reference : references) {
        const auto result =
            build_skeleton(RegionSlowVariables{0, 128, 96}, reference.seed, test_ruleset());
        const auto tiles = populate(result.skeleton, RegionFastVariables{});
        EXPECT_EQ(hash_skeleton(result.skeleton), reference.skeleton) << reference.seed;
        EXPECT_EQ(hash_tiles(tiles), reference.tiles) << reference.seed;
    }
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

TEST(RegionGeneration, MovedTerrainConstantsRemainInTheirParameterHashGroups) {
    const RegionGenerationConfig original;
    auto changed_plate = original;
    ++changed_plate.plates.boundary_spread_distance;
    auto changed_height = original;
    ++changed_height.height.coast_warp_wavelength;
    const auto before = aetheria::worldgen::generation_parameter_hashes(original);
    const auto plate_after = aetheria::worldgen::generation_parameter_hashes(changed_plate);
    const auto height_after = aetheria::worldgen::generation_parameter_hashes(changed_height);

    for (std::size_t index = 0; index < before.groups.size(); ++index) {
        EXPECT_EQ(before.groups[index] != plate_after.groups[index], index == 0);
        EXPECT_EQ(before.groups[index] != height_after.groups[index], index == 1);
    }
}

}  // namespace

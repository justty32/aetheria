#include "core/rules/ruleset.h"
#include "core/world/region_tiles.h"
#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/worldgen/worldgen_test_support.h"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::ruleset_without_ocean;
using aetheria::tests::copy_data_files;
using aetheria::tests::test_ruleset;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::write_text;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::ErosionStageOutput;
using aetheria::worldgen::ElevationRedistributionParams;
using aetheria::worldgen::grayscale;
using aetheria::worldgen::hash_stage;
using aetheria::worldgen::land_fraction;
using aetheria::worldgen::land_is_single_component;
using aetheria::worldgen::MoistureRedistributionParams;
using aetheria::worldgen::populate;
using aetheria::worldgen::quantize_elevation;
using aetheria::worldgen::redistribute;
using aetheria::worldgen::RegionFastVariables;
using aetheria::worldgen::RegionSlowVariables;
using aetheria::worldgen::RiverStageOutput;

[[nodiscard]] aetheria::rules::Ruleset ruleset_with_forest_requirement(
    std::string_view required_terrain) {
    TemporaryDirectory directory;
    copy_data_files(directory.path());
    const auto path = directory.path() / "feature.toml";
    std::ifstream input{path};
    if (!input.is_open()) {
        throw std::runtime_error{"feature fixture open failed"};
    }
    std::string text{std::istreambuf_iterator<char>{input},
                     std::istreambuf_iterator<char>{}};
    constexpr std::string_view declaration =
        "required_terrain = \"terrain.grassland\"";
    const auto position = text.find(declaration);
    if (position == std::string::npos) {
        throw std::runtime_error{"feature fixture required_terrain missing"};
    }
    text.replace(position, declaration.size(),
                 "required_terrain = \"" + std::string{required_terrain} + "\"");
    write_text(path, text);
    return aetheria::rules::RulesetLoader::load(directory.path());
}

TEST(RegionGeneration, MissingRequiredStringIdFailsBeforeGenerating) {
    const auto missing = ruleset_without_ocean();
    try {
        static_cast<void>(build_skeleton(RegionSlowVariables{0, 128, 96}, UINT64_C(1), missing));
        FAIL() << "missing terrain.ocean should throw";
    } catch (const std::invalid_argument& error) {
        EXPECT_NE(std::string{error.what()}.find("terrain.ocean"), std::string::npos);
    }
}

TEST(RegionGeneration, GeneratedFeaturesRespectRequiredTerrain) {
    const auto& ruleset = test_ruleset();
    const auto result =
        build_skeleton(RegionSlowVariables{0, 128, 96}, UINT64_C(515151), ruleset);
    const auto forest = *ruleset.find_feature("feature.forest");
    const auto grassland = *ruleset.find_terrain("terrain.grassland");
    std::size_t forests{};
    for (std::size_t index = 0; index < result.features.feature.size(); ++index) {
        if (result.features.feature[index] == forest) {
            ++forests;
            EXPECT_EQ(result.biome.terrain[index], grassland);
        }
    }
    EXPECT_GT(forests, 0U);
}

TEST(RegionGeneration, ImpossibleRequiredTerrainFailsBeforeFeaturePlacement) {
    const auto ruleset = ruleset_with_forest_requirement("terrain.ocean");
    try {
        static_cast<void>(
            build_skeleton(RegionSlowVariables{0, 128, 96}, UINT64_C(515151), ruleset));
        FAIL() << "land-only forest requirement should throw";
    } catch (const std::runtime_error& error) {
        const std::string message{error.what()};
        EXPECT_NE(message.find("feature.forest"), std::string::npos);
        EXPECT_NE(message.find("terrain.ocean"), std::string::npos);
    }
}

}  // namespace

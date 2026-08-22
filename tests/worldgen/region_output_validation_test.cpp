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

TEST(RegionGeneration, QuantizationIsTheTypedGatewayIntoRegionTiles) {
    const ErosionStageOutput erosion{2, 2, {-5000.0, -0.4, 100.6, 70000.0}, {0, 0, 1, 1}, 0.0};
    const auto quantized = quantize_elevation(erosion);

    EXPECT_EQ(quantized.sea_level, 4096);
    EXPECT_EQ(quantized.meters, (std::vector<std::uint16_t>{0, 4095, 4197, UINT16_MAX}));
    EXPECT_LT(quantized.meters.at(1), quantized.sea_level);
    EXPECT_GT(quantized.meters.at(2), quantized.sea_level);
}

TEST(RegionGeneration, ElevationRedistributionSeamIsIdentityUnlessATestTransformIsInjected) {
    const ErosionStageOutput raw{2, 1, {100.0, 200.0}, {1, 1}, 0.0};
    const auto identity = redistribute(raw, ElevationRedistributionParams{});
    const auto shifted = redistribute(
        raw, ElevationRedistributionParams{},
        [](ErosionStageOutput& field, const ElevationRedistributionParams&) {
            field.elevation.front() += 100.0;
        });

    EXPECT_EQ(hash_stage(identity), hash_stage(raw));
    EXPECT_NE(quantize_elevation(shifted).meters, quantize_elevation(identity).meters);
}

TEST(RegionGeneration, MoistureRedistributionSeamIsIdentityUnlessATestTransformIsInjected) {
    RiverStageOutput raw{2, 1, {4096, 4096}, {-1, -1}, {1, 1}, {0, 0}, {1000, 2000}, {0, 0}};
    const auto identity = redistribute(raw, MoistureRedistributionParams{});
    const auto shifted = redistribute(
        raw, MoistureRedistributionParams{},
        [](RiverStageOutput& field, const MoistureRedistributionParams&) {
            ++field.moisture.front();
        });

    EXPECT_EQ(hash_stage(identity), hash_stage(raw));
    EXPECT_NE(hash_stage(shifted), hash_stage(identity));
}

TEST(RegionGeneration, ProducesTargetLandRatioAndOneConnectedMainland) {
    const auto result =
        build_skeleton(RegionSlowVariables{42, 128, 96}, UINT64_C(555), test_ruleset());
    const auto fraction = land_fraction(result.skeleton);

    EXPECT_GE(fraction, 0.25);
    EXPECT_LE(fraction, 0.35);
    EXPECT_TRUE(land_is_single_component(result.skeleton));
    EXPECT_GE(result.plates.plates.size(), 8U);
    EXPECT_LE(result.plates.plates.size(), 16U);
}

TEST(RegionGeneration, EveryStageHasAFullSizeVisualization) {
    const auto result =
        build_skeleton(RegionSlowVariables{3, 128, 96}, UINT64_C(777), test_ruleset());
    constexpr std::size_t expected = 128U * 96U;

    EXPECT_EQ(grayscale(result.plates).size(), expected);
    EXPECT_EQ(grayscale(result.height).size(), expected);
    EXPECT_EQ(grayscale(result.erosion).size(), expected);
    EXPECT_EQ(grayscale(result.climate).size(), expected);
    EXPECT_EQ(grayscale(result.rivers).size(), expected);
    EXPECT_EQ(grayscale(result.biome).size(), expected);
    EXPECT_EQ(grayscale(result.features).size(), expected);
    EXPECT_EQ(grayscale(result.history).size(), expected);
    EXPECT_EQ(grayscale(result.cities).size(), expected);
    EXPECT_EQ(grayscale(result.roads).size(), expected);
    EXPECT_EQ(grayscale(result.portals).size(), expected);
    EXPECT_EQ(grayscale(result.factions).size(), expected);
}

TEST(RegionGeneration, EveryRiverPathTerminatesAtSeaOrLakeAndEdgesAreSymmetric) {
    const auto result =
        build_skeleton(RegionSlowVariables{7, 128, 96}, UINT64_C(20260816), test_ruleset());
    const auto tiles = populate(result.skeleton, RegionFastVariables{});
    const auto count = result.rivers.river_class.size();
    std::size_t river_tiles{};
    for (std::size_t source = 0; source < count; ++source) {
        if (result.rivers.river_class[source] == 0) {
            continue;
        }
        ++river_tiles;
        auto current = source;
        std::size_t steps{};
        while (result.skeleton.elevation.land[current] != 0 &&
               result.rivers.downstream[current] >= 0) {
            const auto target = static_cast<std::size_t>(result.rivers.downstream[current]);
            ASSERT_LT(target, count);
            ASSERT_LT(++steps, count) << "river cycle from tile " << source;
            if (result.skeleton.elevation.land[target] != 0 &&
                result.rivers.lake[target] == 0) {
                EXPECT_NE(result.rivers.river_class[target], 0)
                    << "river discontinuity after tile " << current;
            }
            if (current == source || result.rivers.river_class[current] != 0) {
                const aetheria::world::RegionXY from{
                    static_cast<std::int16_t>(current % tiles.width),
                    static_cast<std::int16_t>(current / tiles.width)};
                const aetheria::world::RegionXY to{static_cast<std::int16_t>(target % tiles.width),
                                                   static_cast<std::int16_t>(target / tiles.width)};
                EXPECT_EQ(tiles.edge_between(from, to), tiles.edge_between(to, from));
                EXPECT_NE(tiles.edge_between(from, to), result.skeleton.definitions.no_edge);
            }
            current = target;
        }
        EXPECT_TRUE(result.skeleton.elevation.land[current] == 0 ||
                    result.rivers.lake[current] != 0);
    }
    EXPECT_GT(river_tiles, 0U);
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

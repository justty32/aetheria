#include "core/rules/ruleset.h"
#include "core/world/region_tiles.h"
#include "core/worldgen/region_generator.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::Ruleset;
using aetheria::rules::RulesetLoader;
using aetheria::worldgen::ErosionGenerationConfig;
using aetheria::worldgen::ErosionStageOutput;
using aetheria::worldgen::RegionBuildResult;
using aetheria::worldgen::RegionFastVariables;
using aetheria::worldgen::RegionGenerationConfig;
using aetheria::worldgen::RegionSkeleton;
using aetheria::worldgen::RegionSlowVariables;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::erode_height;
using aetheria::worldgen::generate_height;
using aetheria::worldgen::generate_plates;
using aetheria::worldgen::grayscale;
using aetheria::worldgen::hash_skeleton;
using aetheria::worldgen::hash_stage;
using aetheria::worldgen::hash_tiles;
using aetheria::worldgen::land_fraction;
using aetheria::worldgen::land_is_single_component;
using aetheria::worldgen::populate;
using aetheria::worldgen::quantize_elevation;

[[nodiscard]] const Ruleset& test_ruleset() {
    static const auto ruleset =
        RulesetLoader::load(std::filesystem::path{AETHERIA_SOURCE_DIR} / "data");
    return ruleset;
}

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        static std::uint64_t serial{};
        path_ = std::filesystem::temp_directory_path() /
                ("aetheria-worldgen-test-" + std::to_string(++serial));
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() { std::filesystem::remove_all(path_); }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

void write_text(const std::filesystem::path& path, std::string_view text) {
    std::ofstream stream{path};
    ASSERT_TRUE(stream.is_open());
    stream << text;
    ASSERT_TRUE(stream.good());
}

[[nodiscard]] Ruleset ruleset_without_ocean() {
    TemporaryDirectory directory;
    write_text(directory.path() / "terrain.toml", R"(
[[defs]]
id = "terrain.grassland"
name_key = "terrain.grassland.name"
move_cost = 1
flags = 1
visual = "terrain/grassland"
yield = { food = 2, production = 1, wealth = 0, mana = 0 }
)");
    write_text(directory.path() / "relief.toml", R"(
[[defs]]
id = "relief.plain"
name_key = "relief.plain.name"
move_cost = 1
flags = 0
visual = "relief/plain"
)");
    write_text(directory.path() / "feature.toml", R"(
[[defs]]
id = "feature.none"
name_key = "feature.none.name"
move_cost = 1
flags = 0
visual = "feature/none"
)");
    write_text(directory.path() / "edges.toml", R"(
[[defs]]
id = "edge.none"
name_key = "edge.none.name"
move_cost = 1
flags = 0
visual = "edge/none"
)");
    return RulesetLoader::load(directory.path());
}

static_assert(std::is_invocable_r_v<RegionBuildResult, decltype(&build_skeleton),
                                    const RegionSlowVariables&, std::uint64_t,
                                    const Ruleset&, const RegionGenerationConfig&>);
static_assert(!std::is_invocable_v<decltype(&build_skeleton), const RegionFastVariables&,
                                   std::uint64_t, const Ruleset&,
                                   const RegionGenerationConfig&>);
static_assert(std::is_invocable_r_v<aetheria::world::RegionTiles, decltype(&populate),
                                    const RegionSkeleton&, const RegionFastVariables&>);
static_assert(!std::is_invocable_v<decltype(&populate), const ErosionStageOutput&,
                                   const RegionFastVariables&>);
static_assert(sizeof(aetheria::world::RegionTiles) ==
              aetheria::world::kDeclaredRegionTilesStorageSize);

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

TEST(RegionGeneration, SameSeedIsBitIdenticalAcrossEveryStageAndWorldField) {
    const RegionSlowVariables slow{17, 128, 96};
    const auto first = build_skeleton(slow, UINT64_C(12345), test_ruleset());
    const auto second = build_skeleton(slow, UINT64_C(12345), test_ruleset());
    const auto first_tiles = populate(first.skeleton, RegionFastVariables{});
    const auto second_tiles = populate(second.skeleton, RegionFastVariables{});

    EXPECT_EQ(hash_stage(first.plates), hash_stage(second.plates));
    EXPECT_EQ(hash_stage(first.height), hash_stage(second.height));
    EXPECT_EQ(hash_stage(first.erosion), hash_stage(second.erosion));
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

TEST(RegionGeneration, QuantizationIsTheTypedGatewayIntoRegionTiles) {
    const ErosionStageOutput erosion{2,
                                     2,
                                     {-5000.0, -0.4, 100.6, 70000.0},
                                     {0, 0, 1, 1},
                                     0.0};
    const auto quantized = quantize_elevation(erosion);

    EXPECT_EQ(quantized.sea_level, 4096);
    EXPECT_EQ(quantized.meters,
              (std::vector<std::uint16_t>{0, 4095, 4197, UINT16_MAX}));
    EXPECT_LT(quantized.meters.at(1), quantized.sea_level);
    EXPECT_GT(quantized.meters.at(2), quantized.sea_level);
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
}

TEST(RegionGeneration, MissingRequiredStringIdFailsBeforeGenerating) {
    const auto missing = ruleset_without_ocean();
    try {
        static_cast<void>(
            build_skeleton(RegionSlowVariables{0, 128, 96}, UINT64_C(1), missing));
        FAIL() << "missing terrain.ocean should throw";
    } catch (const std::invalid_argument& error) {
        EXPECT_NE(std::string{error.what()}.find("terrain.ocean"), std::string::npos);
    }
}

TEST(RegionGeneration, DefaultRegionFitsThreeSecondBudget) {
    const auto start = std::chrono::steady_clock::now();
    const auto result =
        build_skeleton(RegionSlowVariables{9, 128, 96}, UINT64_C(20260816), test_ruleset());
    const auto tiles = populate(result.skeleton, RegionFastVariables{});
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_TRUE(tiles.valid_layout());
    EXPECT_LT(elapsed, std::chrono::seconds{3});
}

TEST(RegionGeneration, StageAndRegionSeedsAreIndependent) {
    const auto plate = aetheria::worldgen::derive_stage_seed(UINT64_C(77), UINT64_C(1));
    const auto height = aetheria::worldgen::derive_stage_seed(UINT64_C(77), UINT64_C(2));
    const auto region_a = aetheria::worldgen::derive_region_seed(UINT64_C(77), 4);
    const auto region_b = aetheria::worldgen::derive_region_seed(UINT64_C(77), 5);

    EXPECT_NE(plate, height);
    EXPECT_NE(region_a, region_b);
}

}  // namespace

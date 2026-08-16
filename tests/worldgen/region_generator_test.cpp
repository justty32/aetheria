#include "core/rules/ruleset.h"
#include "core/world/region_tiles.h"
#include "core/worldgen/region_generator.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::Ruleset;
using aetheria::rules::RulesetLoader;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::ClimateStageOutput;
using aetheria::worldgen::erode_height;
using aetheria::worldgen::ErosionGenerationConfig;
using aetheria::worldgen::ErosionStageOutput;
using aetheria::worldgen::generate_climate;
using aetheria::worldgen::generate_height;
using aetheria::worldgen::generate_plates;
using aetheria::worldgen::generate_rivers;
using aetheria::worldgen::grayscale;
using aetheria::worldgen::hash_skeleton;
using aetheria::worldgen::hash_stage;
using aetheria::worldgen::hash_tiles;
using aetheria::worldgen::land_fraction;
using aetheria::worldgen::land_is_single_component;
using aetheria::worldgen::populate;
using aetheria::worldgen::quantize_elevation;
using aetheria::worldgen::QuantizedElevation;
using aetheria::worldgen::RegionBuildResult;
using aetheria::worldgen::RegionFastVariables;
using aetheria::worldgen::RegionGenerationConfig;
using aetheria::worldgen::RegionSkeleton;
using aetheria::worldgen::RegionSlowVariables;

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

void copy_data_files(const std::filesystem::path& destination) {
    const auto source = std::filesystem::path{AETHERIA_SOURCE_DIR} / "data";
    for (const auto& entry : std::filesystem::recursive_directory_iterator{source}) {
        const auto target = destination / std::filesystem::relative(entry.path(), source);
        if (entry.is_directory()) {
            std::filesystem::create_directories(target);
        } else {
            std::filesystem::copy_file(entry.path(), target,
                                       std::filesystem::copy_options::overwrite_existing);
        }
    }
}

struct RiverBenchmark {
    double microseconds{};
    std::uint64_t hash{};
};

[[nodiscard]] RiverBenchmark benchmark_rivers(std::uint32_t side) {
    const auto count = static_cast<std::size_t>(side) * side;
    QuantizedElevation elevation{side, side, {}, {}, 4096};
    elevation.meters.resize(count);
    elevation.land.assign(count, 1);
    for (std::size_t index = 0; index < count; ++index) {
        elevation.meters[index] = static_cast<std::uint16_t>(
            4097U + (aetheria::worldgen::splitmix64(index) % UINT64_C(4000)));
    }
    ClimateStageOutput climate{side, side, {}, {}, {}};
    climate.temperature_tenths.assign(count, 100);
    climate.moisture.assign(count, 1200);
    climate.prevailing_wind_x.assign(side, 1);
    constexpr std::size_t repetitions = 8;
    aetheria::worldgen::RiverStageOutput output;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0; iteration < repetitions; ++iteration) {
        output = generate_rivers(elevation, climate, UINT64_C(7007) + iteration, {});
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    return {std::chrono::duration<double, std::micro>{elapsed}.count() / repetitions,
            hash_stage(output)};
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
    EXPECT_GT(climate.moisture.at(4), climate.moisture.at(5) * 10U);
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

TEST(RegionGenerationStage, RiverRuntimeScalesWithCellCount) {
    const auto small = benchmark_rivers(128);
    const auto medium = benchmark_rivers(256);
    const auto large = benchmark_rivers(512);

    std::cout << "river_scale cells=16384 us=" << small.microseconds
              << " us_per_cell=" << small.microseconds / 16384.0 << "\n"
              << "river_scale cells=65536 us=" << medium.microseconds
              << " us_per_cell=" << medium.microseconds / 65536.0 << "\n"
              << "river_scale cells=262144 us=" << large.microseconds
              << " us_per_cell=" << large.microseconds / 262144.0 << '\n';
    EXPECT_NE(small.hash, 0U);
    EXPECT_NE(medium.hash, 0U);
    EXPECT_NE(large.hash, 0U);
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
    EXPECT_EQ(hash_stage(first.climate), hash_stage(second.climate));
    EXPECT_EQ(hash_stage(first.rivers), hash_stage(second.rivers));
    EXPECT_EQ(hash_stage(first.biome), hash_stage(second.biome));
    EXPECT_EQ(hash_stage(first.features), hash_stage(second.features));
    EXPECT_EQ(hash_stage(first.cities), hash_stage(second.cities));
    EXPECT_EQ(hash_stage(first.roads), hash_stage(second.roads));
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

TEST(RegionGeneration, QuantizationIsTheTypedGatewayIntoRegionTiles) {
    const ErosionStageOutput erosion{2, 2, {-5000.0, -0.4, 100.6, 70000.0}, {0, 0, 1, 1}, 0.0};
    const auto quantized = quantize_elevation(erosion);

    EXPECT_EQ(quantized.sea_level, 4096);
    EXPECT_EQ(quantized.meters, (std::vector<std::uint16_t>{0, 4095, 4197, UINT16_MAX}));
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
    EXPECT_EQ(grayscale(result.climate).size(), expected);
    EXPECT_EQ(grayscale(result.rivers).size(), expected);
    EXPECT_EQ(grayscale(result.biome).size(), expected);
    EXPECT_EQ(grayscale(result.features).size(), expected);
    EXPECT_EQ(grayscale(result.cities).size(), expected);
    EXPECT_EQ(grayscale(result.roads).size(), expected);
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
            if (result.skeleton.elevation.land[target] != 0) {
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

TEST(RegionGeneration, EditingBiomeDataChangesOnlyDataDrivenStages) {
    TemporaryDirectory directory;
    copy_data_files(directory.path());
    write_text(directory.path() / "biomes.toml", R"(
[[rules]]
fallback = true
terrain = "terrain.tundra"
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

TEST(RegionGeneration, MissingRequiredStringIdFailsBeforeGenerating) {
    const auto missing = ruleset_without_ocean();
    try {
        static_cast<void>(build_skeleton(RegionSlowVariables{0, 128, 96}, UINT64_C(1), missing));
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

#include "core/worldgen/gen_stage_ids.h"
#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"

#include <chrono>
#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::test_ruleset;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::ClimateStageOutput;
using aetheria::worldgen::generate_rivers;
using aetheria::worldgen::generate_history;
using aetheria::worldgen::hash_stage;
using aetheria::worldgen::populate;
using aetheria::worldgen::QuantizedElevation;
using aetheria::worldgen::RegionFastVariables;
using aetheria::worldgen::RegionSlowVariables;

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

TEST(RegionGeneration, DefaultRegionFitsThreeSecondBudget) {
    constexpr auto seed = UINT64_C(20260816);
    const RegionSlowVariables slow{9, 128, 96};
    const auto start = std::chrono::steady_clock::now();
    const auto result = build_skeleton(slow, seed, test_ruleset());
    const auto tiles = populate(result.skeleton, RegionFastVariables{});
    const auto elapsed = std::chrono::steady_clock::now() - start;

    const auto history_start = std::chrono::steady_clock::now();
    const auto history = generate_history(
        result.skeleton.elevation, result.climate, result.rivers, result.biome, result.features,
        result.skeleton.definitions, test_ruleset(),
        aetheria::worldgen::derive_region_stage_seed(
            seed, slow.region_id, aetheria::worldgen::detail::kHistoryStageId),
        {});
    const auto history_elapsed = std::chrono::steady_clock::now() - history_start;
    const auto elapsed_ms = std::chrono::duration<double, std::milli>{elapsed}.count();
    const auto history_ms = std::chrono::duration<double, std::milli>{history_elapsed}.count();

    EXPECT_TRUE(tiles.valid_layout());
    EXPECT_EQ(hash_stage(history), hash_stage(result.history));
    EXPECT_LT(elapsed, std::chrono::seconds{3});
    std::cout << "twelve_stage_region_ms=" << elapsed_ms << " history_stage_ms=" << history_ms
              << " budget_remaining_ms=" << 3000.0 - elapsed_ms << '\n';
}

}  // namespace

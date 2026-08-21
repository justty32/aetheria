#include "core/worldgen/gen_stage_ids.h"
#include "core/worldgen/region_generator.h"
#include "tests/support/performance.h"
#include "tests/support/ruleset_fixture.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>

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
    aetheria::worldgen::RiverStageOutput output;
    const auto minimum_milliseconds = aetheria::tests::minimum_milliseconds_after_warmup([&] {
        const auto start = std::chrono::steady_clock::now();
        output = generate_rivers(elevation, climate, UINT64_C(7007), {});
        const auto elapsed = std::chrono::steady_clock::now() - start;
        return std::chrono::duration<double, std::milli>{elapsed}.count();
    });
    return {minimum_milliseconds * 1000.0, hash_stage(output)};
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
    std::optional<decltype(build_skeleton(slow, seed, test_ruleset()))> result;
    const auto minimum_milliseconds = aetheria::tests::minimum_milliseconds_after_warmup([&] {
        const auto start = std::chrono::steady_clock::now();
        auto measured = build_skeleton(slow, seed, test_ruleset());
        const auto tiles = populate(measured.skeleton, RegionFastVariables{});
        const auto elapsed = std::chrono::steady_clock::now() - start;
        EXPECT_TRUE(tiles.valid_layout());
        result = std::move(measured);
        return std::chrono::duration<double, std::milli>{elapsed}.count();
    });
    ASSERT_TRUE(result.has_value());
    aetheria::worldgen::HistoryStageOutput history;
    const auto history_minimum_milliseconds =
        aetheria::tests::minimum_milliseconds_after_warmup([&] {
            const auto start = std::chrono::steady_clock::now();
            history = generate_history(
                result->skeleton.elevation, result->climate, result->rivers, result->biome,
                result->features, result->skeleton.definitions, test_ruleset(),
                aetheria::worldgen::derive_region_stage_seed(
                    seed, slow.region_id, aetheria::worldgen::detail::kHistoryStageId),
                {});
            const auto elapsed = std::chrono::steady_clock::now() - start;
            return std::chrono::duration<double, std::milli>{elapsed}.count();
        });

    EXPECT_EQ(hash_stage(history), hash_stage(result->history));
    EXPECT_LT(minimum_milliseconds, 3000.0);
    std::cout << "twelve_stage_region_min_of_5_ms=" << minimum_milliseconds
              << " history_stage_min_of_5_ms=" << history_minimum_milliseconds
              << " budget_remaining_ms=" << 3000.0 - minimum_milliseconds << '\n';
}

}  // namespace

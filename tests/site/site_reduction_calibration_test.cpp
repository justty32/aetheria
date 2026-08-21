#include "core/site/site_materialize.h"
#include "core/site/site_reduction.h"
#include "core/world/region_simulation.h"
#include "core/worldgen/region_seed.h"
#include "tests/site/site_reduction_test_support.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::site::ReductionTable;
using aetheria::tests::kReductionCoordinate;
using aetheria::tests::kReductionRegionId;
using aetheria::tests::kReductionWorldSeed;
using aetheria::tests::reduction_region;
using aetheria::tests::test_ruleset;
using aetheria::world::DevelopmentLevelReduction;
using aetheria::world::PopulationReduction;
using aetheria::world::SettlementTier;

struct ErrorDistribution {
    double minimum{};
    double median{};
    double p95{};
    double maximum{};
    double over_five_percent{};
};

[[nodiscard]] ErrorDistribution summarize(std::vector<double> errors) {
    std::ranges::sort(errors);
    const auto over = std::ranges::count_if(errors, [](double error) { return error >= 0.05; });
    return {.minimum = errors.front(),
            .median = errors[errors.size() / 2U],
            .p95 = errors[errors.size() * 95U / 100U],
            .maximum = errors.back(),
            .over_five_percent = static_cast<double>(over) / errors.size()};
}

[[nodiscard]] double relative_error(std::uint32_t actual, std::uint32_t expected) {
    if (expected == 0U) {
        return actual == 0U ? 0.0 : 1.0;
    }
    return std::abs(static_cast<double>(actual) - expected) / expected;
}

TEST(SiteReduction, ThousandRandomRegionStatesStayCalibrated) {
    constexpr std::size_t sample_count = 1000;
    const auto& ruleset = test_ruleset();
    std::vector<double> population_errors;
    std::vector<double> development_errors;
    population_errors.reserve(sample_count);
    development_errors.reserve(sample_count);
    for (std::size_t sample = 0; sample < sample_count; ++sample) {
        const auto random = aetheria::worldgen::splitmix64(UINT64_C(0x4D2C4) ^ sample);
        auto tiles = reduction_region(static_cast<SettlementTier>(random % 4U));
        if (tiles.settlement[0] == SettlementTier::None) {
            tiles.base[0] =
                static_cast<aetheria::rules::TerrainId>(random % ruleset.terrains().size());
        }
        tiles.relief[0] = static_cast<aetheria::rules::ReliefId>(
            (random >> 8U) % ruleset.reliefs().size());
        tiles.feature[0] = static_cast<aetheria::rules::FeatureId>(
            (random >> 16U) % ruleset.features().size());
        tiles.elevation[0] = static_cast<std::uint16_t>(random >> 32U);
        tiles.owner[0] = static_cast<aetheria::world::FactionId>(random >> 48U);
        auto site = aetheria::site::materialize_site_zone(
            tiles, kReductionCoordinate, kReductionWorldSeed ^ random, kReductionRegionId,
            ruleset);
        const auto delta = ReductionTable::reduce(
            std::get<aetheria::zone::SitePayload>(site.payload).layers);
        const auto formula = aetheria::world::region_formula(tiles.settlement[0]);
        population_errors.push_back(
            relative_error(delta.value<PopulationReduction>(), formula.population));
        development_errors.push_back(relative_error(
            delta.value<DevelopmentLevelReduction>(), formula.development_level));
    }
    const auto population = summarize(std::move(population_errors));
    const auto development = summarize(std::move(development_errors));
    std::cout << "calibration_population min=" << population.minimum
              << " median=" << population.median << " p95=" << population.p95
              << " max=" << population.maximum << " over_5pct="
              << population.over_five_percent << '\n'
              << "calibration_development min=" << development.minimum
              << " median=" << development.median << " p95=" << development.p95
              << " max=" << development.maximum << " over_5pct="
              << development.over_five_percent << '\n';
    EXPECT_LT(population.maximum, 0.05);
    EXPECT_LT(development.maximum, 0.05);
}

TEST(SiteReduction, ReductionFitsThirtyMillisecondBudget) {
    auto tiles = reduction_region();
    const auto site = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, test_ruleset());
    aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, site);
    const auto start = std::chrono::steady_clock::now();
    aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, site);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const auto milliseconds = std::chrono::duration<double, std::milli>{elapsed}.count();
#ifdef NDEBUG
    constexpr auto build_kind = "Release";
#else
    constexpr auto build_kind = "Debug";
#endif
    std::cout << "site_reduction_" << build_kind << "_ms=" << milliseconds << '\n';
    EXPECT_EQ(tiles.reduction_value<PopulationReduction>(kReductionCoordinate), 100U);
    EXPECT_LT(elapsed, std::chrono::milliseconds{30});
}

}  // namespace

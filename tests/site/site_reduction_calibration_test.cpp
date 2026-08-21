#include "core/site/site_build_loop.h"
#include "core/site/site_materialize.h"
#include "core/site/site_reduction.h"
#include "core/world/region_simulation.h"
#include "core/worldgen/region_seed.h"
#include "tests/site/site_reduction_test_support.h"
#include "tests/support/performance.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::kReductionCoordinate;
using aetheria::tests::kReductionRegionId;
using aetheria::tests::kReductionWorldSeed;
using aetheria::tests::reduction_region;
using aetheria::tests::test_ruleset;
using aetheria::world::DevelopmentLevelReduction;
using aetheria::world::PopulationReduction;
using aetheria::world::SettlementTier;

struct Quantities {
    std::uint32_t population{};
    std::uint16_t development{};
    std::uint64_t food{};
    std::uint64_t production{};
};

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

template <typename Actual, typename Expected>
[[nodiscard]] double relative_error(Actual actual, Expected expected) {
    if (expected == 0U) {
        return actual == 0U ? 0.0 : 1.0;
    }
    return std::abs(static_cast<double>(actual) - expected) / expected;
}

[[nodiscard]] Quantities run_site_formula(aetheria::world::RegionTiles tiles,
                                          std::uint64_t world_seed, std::uint32_t xun,
                                          const Quantities& initial) {
    auto site = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, world_seed, kReductionRegionId, test_ruleset());
    aetheria::zone::Zone region{
        aetheria::zone::child_key(aetheria::zone::kRootZone, kReductionRegionId, 0)};
    auto& region_tiles = std::get<aetheria::zone::RegionPayload>(region.payload)
                             .layers.emplace(0, std::move(tiles)).first->second;
    const auto meta = *region.reg.view<aetheria::zone::ZoneMeta>().begin();
    region.reg.emplace<aetheria::world::TurnClock>(meta);
    aetheria::site::enter_full_site(site, region_tiles, kReductionCoordinate);
    auto& state = aetheria::site::city_build_state(site);
    state.buildings = {{"city.house", {10, 10}},
                       {"city.farm", {40, 40}},
                       {"city.square", {12, 10}},
                       {"city.workshop", {20, 20}},
                       {"city.mine", {23, 20}}};
    state.economy.population = initial.population;
    state.economy.food_stock = initial.food;
    state.economy.production_stock = initial.production;
    state.economy.population_micro_remainder = 0;
    aetheria::site::reduce_live_site_xun(region_tiles, kReductionCoordinate, site);
    aetheria::zone::InMemoryZoneStore store{test_ruleset()};
    aetheria::site::SiteTurnPipeline pipeline{test_ruleset(), store};
    static_cast<void>(pipeline.advance_hours(site, region, 0, kReductionCoordinate, xun * 240U));
    return {
        region_tiles.reduction_value<PopulationReduction>(kReductionCoordinate),
        region_tiles.reduction_value<DevelopmentLevelReduction>(kReductionCoordinate),
        region_tiles.reduction_value<aetheria::world::FoodStockReduction>(kReductionCoordinate),
        region_tiles.reduction_value<aetheria::world::ProductionStockReduction>(
            kReductionCoordinate),
    };
}

[[nodiscard]] Quantities run_region_formula(SettlementTier settlement, std::uint32_t xun,
                                            Quantities state) {
    for (std::uint32_t step = 0; step < xun; ++step) {
        const auto next = aetheria::world::region_formula(
            settlement, state.population, state.food, state.production);
        state = {next.population, next.development_level, next.food_stock,
                 next.production_stock};
    }
    return state;
}

[[nodiscard]] double maximum_error(const Quantities& actual, const Quantities& expected) {
    return std::max({relative_error(actual.population, expected.population),
                     relative_error(actual.development, expected.development),
                     relative_error(actual.food, expected.food),
                     relative_error(actual.production, expected.production)});
}

TEST(SiteReduction, ThousandRandomRegionStatesStayCalibrated) {
    constexpr std::size_t sample_count = 1000;
    const auto& ruleset = test_ruleset();
    std::vector<double> errors;
    errors.reserve(sample_count);
    std::size_t nonzero_errors{};
    for (std::size_t sample = 0; sample < sample_count; ++sample) {
        const auto random = aetheria::worldgen::splitmix64(UINT64_C(0x4D2C4) ^ sample);
        const auto settlement = static_cast<SettlementTier>(1U + random % 3U);
        auto tiles = reduction_region(settlement);
        tiles.relief[0] = static_cast<aetheria::rules::ReliefId>(
            (random >> 8U) % ruleset.reliefs().size());
        tiles.feature[0] = static_cast<aetheria::rules::FeatureId>(
            (random >> 16U) % ruleset.features().size());
        tiles.elevation[0] = static_cast<std::uint16_t>(random >> 32U);
        tiles.owner[0] = static_cast<aetheria::world::FactionId>(random >> 48U);
        const Quantities initial{
            .population = static_cast<std::uint32_t>(25U + ((random >> 20U) % 426U)),
            .development = 1,
            .food = (random >> 12U) % 20'000U,
            .production = (random >> 28U) % 40'000U,
        };
        constexpr std::uint32_t calibration_xun = 1;
        const auto actual = run_site_formula(
            tiles, kReductionWorldSeed ^ random, calibration_xun, initial);
        const auto expected = run_region_formula(settlement, calibration_xun, initial);
        const auto error = maximum_error(actual, expected);
        errors.push_back(error);
        nonzero_errors += error > 0.0 ? 1U : 0U;
    }
    const auto distribution = summarize(std::move(errors));
    std::cout << "calibration_real samples=" << sample_count << " xun_per_sample=1"
              << " nonzero=" << nonzero_errors << " min=" << distribution.minimum
              << " median=" << distribution.median << " p95=" << distribution.p95
              << " max=" << distribution.maximum << " over_5pct="
              << distribution.over_five_percent << '\n';
    EXPECT_GT(nonzero_errors, 0U);
    EXPECT_LT(distribution.maximum, 0.05);
    EXPECT_EQ(distribution.over_five_percent, 0.0);
}

TEST(SiteReduction, ReductionFitsThirtyMillisecondBudget) {
    auto tiles = reduction_region();
    const auto site = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, test_ruleset());
    const auto minimum_milliseconds = aetheria::tests::minimum_milliseconds_after_warmup([&] {
        const auto start = std::chrono::steady_clock::now();
        aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, site);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        return std::chrono::duration<double, std::milli>{elapsed}.count();
    });
#ifdef NDEBUG
    constexpr auto build_kind = "Release";
#else
    constexpr auto build_kind = "Debug";
#endif
    std::cout << "site_reduction_" << build_kind << "_min_of_5_ms="
              << minimum_milliseconds << '\n';
    EXPECT_EQ(tiles.reduction_value<PopulationReduction>(kReductionCoordinate), 100U);
    EXPECT_LT(minimum_milliseconds, 30.0);
}

}  // namespace

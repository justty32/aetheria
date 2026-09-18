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
#include <limits>
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

TEST(SiteReduction, ReductionFitsThirtyMillisecondBudget) {
    auto tiles = reduction_region();
    const auto site = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, test_ruleset());
    const auto minimum_milliseconds = aetheria::tests::minimum_milliseconds_after_warmup([&] {
        const auto start = std::chrono::steady_clock::now();
        aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, site, test_ruleset());
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

#include "tests/site/site_fill_test_support.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::site::SiteFastVars;
using aetheria::site::SiteZoning;
using aetheria::tests::fill_counts;
using aetheria::tests::fill_slow_vars;
using aetheria::tests::fill_tile_index;
using aetheria::tests::frontage_is_road;
using aetheria::tests::generate_fill;
using aetheria::tests::test_ruleset;

TEST(SiteFill, FastVariablesChangeFillHashButNeverSkeletonHash) {
    auto baseline_fast = SiteFastVars{};
    baseline_fast.settlement = aetheria::world::SettlementTier::Town;
    baseline_fast.population = 250;
    baseline_fast.development_level = 1;
    auto population_fast = baseline_fast;
    population_fast.population = 2000;
    auto development_fast = baseline_fast;
    development_fast.development_level = 10;

    const auto baseline = generate_fill(baseline_fast);
    const auto population = generate_fill(population_fast);
    const auto development = generate_fill(development_fast);
    const auto baseline_s = aetheria::site::hash_site_skeleton(baseline.skeleton);
    const auto population_s = aetheria::site::hash_site_skeleton(population.skeleton);
    const auto development_s = aetheria::site::hash_site_skeleton(development.skeleton);
    const auto baseline_f = aetheria::site::hash_site_fill(baseline);
    const auto population_f = aetheria::site::hash_site_fill(population);
    const auto development_f = aetheria::site::hash_site_fill(development);

    EXPECT_EQ(baseline_s, population_s);
    EXPECT_EQ(baseline_s, development_s);
    EXPECT_NE(baseline_f, population_f);
    EXPECT_NE(baseline_f, development_f);
    std::cout << "site_fill_skeleton_hashes baseline=" << baseline_s
              << " population=" << population_s << " development=" << development_s << '\n'
              << "site_fill_output_hashes baseline=" << baseline_f
              << " population=" << population_f << " development=" << development_f << '\n';
}

TEST(SiteFill, VillageAndMetropolisUseQuotasOnTheSamePath) {
    SiteFastVars village_fast{};
    village_fast.settlement = aetheria::world::SettlementTier::Village;
    village_fast.population = 250;
    village_fast.development_level = 1;
    auto metropolis_fast = village_fast;
    metropolis_fast.settlement = aetheria::world::SettlementTier::City;
    metropolis_fast.population = 8000;
    metropolis_fast.development_level = 20;

    const auto village = generate_fill(village_fast);
    const auto metropolis = generate_fill(metropolis_fast);
    const auto small = fill_counts(village);
    const auto large = fill_counts(metropolis);
    EXPECT_EQ(small.residential_blocks, 1U);
    EXPECT_EQ(small.commercial_blocks, 1U);
    EXPECT_GT(large.residential_blocks, small.residential_blocks);
    EXPECT_GT(large.commercial_blocks, small.commercial_blocks);
    EXPECT_GT(large.residential_buildings, small.residential_buildings);
    EXPECT_GT(large.commercial_buildings, small.commercial_buildings);
    std::cout << "site_fill_village residential_blocks=" << small.residential_blocks
              << " commercial_blocks=" << small.commercial_blocks
              << " residential_buildings=" << small.residential_buildings
              << " commercial_buildings=" << small.commercial_buildings << '\n'
              << "site_fill_metropolis residential_blocks=" << large.residential_blocks
              << " commercial_blocks=" << large.commercial_blocks
              << " residential_buildings=" << large.residential_buildings
              << " commercial_buildings=" << large.commercial_buildings << '\n';
}

TEST(SiteFill, BuildingsAreDisjointBuildableStreetFrontageWithCourtyards) {
    SiteFastVars fast{};
    fast.settlement = aetheria::world::SettlementTier::City;
    fast.population = 8000;
    fast.development_level = 20;
    const auto layer = generate_fill(fast);
    std::vector<std::uint8_t> occupied(aetheria::site::kSiteTileCount);
    std::size_t occupied_area{};
    std::size_t zoned_area{};
    for (std::size_t index = 0; index < layer.zoning.size(); ++index) {
        zoned_area += layer.zoning[index] != SiteZoning::Open ? 1U : 0U;
    }
    for (const auto& building : layer.buildings) {
        EXPECT_TRUE(frontage_is_road(layer, building));
        for (std::uint16_t y = building.origin.y; y < building.origin.y + building.height; ++y) {
            for (std::uint16_t x = building.origin.x; x < building.origin.x + building.width; ++x) {
                const auto index = fill_tile_index(x, y);
                EXPECT_EQ(occupied[index], 0U);
                EXPECT_NE(layer.zoning[index], SiteZoning::Open);
                EXPECT_NE(layer.skeleton.buildable[index], 0U);
                occupied[index] = UINT8_C(1);
                ++occupied_area;
            }
        }
    }
    EXPECT_TRUE(layer.valid_layout());
    EXPECT_GT(layer.buildings.size(), 0U);
    EXPECT_LT(occupied_area, zoned_area);
}

TEST(SiteFill, SkeletonAndFillFitThirtyMillisecondBudget) {
    SiteFastVars fast{};
    fast.settlement = aetheria::world::SettlementTier::City;
    fast.population = 100000;
    fast.development_level = 100;
    constexpr auto seed = UINT64_C(0xF1131);
    static_cast<void>(aetheria::site::populate(
        aetheria::site::build_site_skeleton(fill_slow_vars(true), seed, test_ruleset()), fast,
        test_ruleset()));
    const auto start = std::chrono::steady_clock::now();
    const auto result = aetheria::site::populate(
        aetheria::site::build_site_skeleton(fill_slow_vars(true), seed, test_ruleset()), fast,
        test_ruleset());
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const auto milliseconds = std::chrono::duration<double, std::milli>{elapsed}.count();
#ifdef NDEBUG
    constexpr auto kind = "Release";
#else
    constexpr auto kind = "Debug";
#endif
    EXPECT_TRUE(result.valid_layout());
    EXPECT_LT(elapsed, std::chrono::milliseconds{30});
    std::cout << "site_skeleton_fill_" << kind << "_ms=" << milliseconds << '\n';
}

}  // namespace

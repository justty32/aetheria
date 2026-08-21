#include "core/worldgen/region_generator.h"
#include "tests/support/performance.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::test_ruleset;
using aetheria::world::RegionXY;
using aetheria::worldgen::BiomeStageOutput;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::ClimateStageOutput;
using aetheria::worldgen::FeatureStageOutput;
using aetheria::worldgen::generate_cities;
using aetheria::worldgen::score_city_sites;
using aetheria::worldgen::QuantizedElevation;
using aetheria::worldgen::RegionSlowVariables;
using aetheria::worldgen::RiverStageOutput;

[[nodiscard]] std::size_t index_of(std::uint32_t width, RegionXY tile) {
    return static_cast<std::size_t>(tile.y) * width + static_cast<std::size_t>(tile.x);
}

struct CityFixture {
    QuantizedElevation elevation;
    ClimateStageOutput climate;
    RiverStageOutput rivers;
    BiomeStageOutput biome;
    FeatureStageOutput features;
};

[[nodiscard]] CityFixture bottleneck_fixture() {
    constexpr std::uint32_t width = 20;
    constexpr std::uint32_t height = 9;
    constexpr std::size_t count = width * height;
    CityFixture fixture{
        {width, height, std::vector<std::uint16_t>(count, 4090), std::vector<std::uint8_t>(count),
         4096},
        {width, height, std::vector<std::int16_t>(count, 180),
         std::vector<std::uint16_t>(count, 32000), std::vector<std::int8_t>(height, 1)},
        {width, height, std::vector<std::uint16_t>(count, 4090),
         std::vector<std::int32_t>(count, -1), std::vector<std::uint32_t>(count),
         std::vector<std::uint8_t>(count), std::vector<std::uint16_t>(count, 32000),
         std::vector<std::uint8_t>(count)},
        {width, height,
         std::vector<aetheria::rules::TerrainId>(count,
                                                 *test_ruleset().find_terrain("terrain.grassland")),
         std::vector<aetheria::rules::ReliefId>(count,
                                                *test_ruleset().find_relief("relief.plain"))},
        {width, height,
         std::vector<aetheria::rules::FeatureId>(count,
                                                 *test_ruleset().find_feature("feature.none"))}};
    auto make_land = [&](int first_x, int last_x, int first_y, int last_y) {
        for (int y = first_y; y <= last_y; ++y) {
            for (int x = first_x; x <= last_x; ++x) {
                const auto index =
                    static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x);
                fixture.elevation.land[index] = 1;
                fixture.elevation.meters[index] = 4300;
            }
        }
    };
    make_land(1, 6, 1, 7);
    make_land(13, 18, 1, 7);
    make_land(7, 12, 4, 4);
    return fixture;
}

TEST(CityGenerationStage, TerrainBottleneckOutscoresNearbyOpenGround) {
    const auto fixture = bottleneck_fixture();
    const auto cities = score_city_sites(fixture.elevation, fixture.climate, fixture.rivers,
                                         fixture.biome, fixture.features, test_ruleset(),
                                         test_ruleset().civilization_rules().scoring_weights);
    const auto choke = index_of(fixture.elevation.width, {9, 4});
    const auto plain = index_of(fixture.elevation.width, {3, 4});

    std::cout << "bottleneck choke=" << cities.bottleneck[choke]
              << " plain=" << cities.bottleneck[plain] << " choke_score=" << cities.score[choke]
              << " plain_score=" << cities.score[plain] << '\n';
    EXPECT_GT(cities.bottleneck[choke], cities.bottleneck[plain]);
    EXPECT_GT(cities.score[choke], cities.score[plain]);
}

TEST(CityGenerationStage, PassThroughMountainBarrierIsBottleneck) {
    auto fixture = bottleneck_fixture();
    std::ranges::fill(fixture.elevation.land, 1);
    std::ranges::fill(fixture.elevation.meters, 4300);
    const auto mountain = *test_ruleset().find_relief("relief.mountain");
    for (std::int16_t y = 0; y < static_cast<std::int16_t>(fixture.elevation.height); ++y) {
        if (y != 3) {
            fixture.biome.relief[index_of(fixture.elevation.width, {4, y})] = mountain;
        }
    }
    const auto cities = score_city_sites(fixture.elevation, fixture.climate, fixture.rivers,
                                         fixture.biome, fixture.features, test_ruleset(),
                                         test_ruleset().civilization_rules().scoring_weights);
    const auto pass = index_of(fixture.elevation.width, {4, 3});
    const auto north = index_of(fixture.elevation.width, {4, 2});
    const auto south = index_of(fixture.elevation.width, {4, 4});

    std::cout << "mountain_pass score=" << cities.bottleneck[pass]
              << " north_barrier=" << cities.bottleneck[north]
              << " south_barrier=" << cities.bottleneck[south] << '\n';
    EXPECT_GT(cities.bottleneck[pass], 0U);
    EXPECT_EQ(cities.bottleneck[north], 0U);
    EXPECT_EQ(cities.bottleneck[south], 0U);
}

TEST(CityGenerationStage, FullRegionScoresAll12288TilesWithinBudgetAndRespectsSpacing) {
    const auto terrain =
        build_skeleton(RegionSlowVariables{44, 128, 96}, UINT64_C(440044), test_ruleset());
    auto cities =
        generate_cities(terrain.skeleton.elevation, terrain.climate, terrain.rivers, terrain.biome,
                        terrain.history, test_ruleset(), UINT64_C(9009), {});
    const auto minimum_milliseconds = aetheria::tests::minimum_milliseconds_after_warmup([&] {
        const auto start = std::chrono::steady_clock::now();
        cities = generate_cities(terrain.skeleton.elevation, terrain.climate, terrain.rivers,
                                 terrain.biome, terrain.history, test_ruleset(), UINT64_C(9009), {});
        const auto elapsed = std::chrono::steady_clock::now() - start;
        return std::chrono::duration<double, std::milli>{elapsed}.count();
    });

    std::cout << "bottleneck_12288_min_of_5_ms=" << minimum_milliseconds
              << " city_count=" << cities.cities.size() << '\n';
    EXPECT_EQ(cities.score.size(), 12288U);
    EXPECT_EQ(cities.cities.size(), test_ruleset().civilization_rules().target_city_count);
    EXPECT_EQ(std::ranges::count(cities.cities, aetheria::world::SettlementTier::City,
                                 &aetheria::worldgen::CitySite::tier),
              test_ruleset().civilization_rules().major_city_count);
    EXPECT_EQ(std::ranges::count(cities.cities, aetheria::world::SettlementTier::Town,
                                 &aetheria::worldgen::CitySite::tier),
              test_ruleset().civilization_rules().town_count);
    for (std::size_t first = 0; first < cities.cities.size(); ++first) {
        for (std::size_t second = first + 1; second < cities.cities.size(); ++second) {
            const auto dx = std::abs(static_cast<int>(cities.cities[first].tile.x) -
                                     static_cast<int>(cities.cities[second].tile.x));
            const auto dy = std::abs(static_cast<int>(cities.cities[first].tile.y) -
                                     static_cast<int>(cities.cities[second].tile.y));
            EXPECT_GE(dx + dy, std::max(cities.cities[first].minimum_spacing,
                                        cities.cities[second].minimum_spacing));
        }
    }
}

}  // namespace

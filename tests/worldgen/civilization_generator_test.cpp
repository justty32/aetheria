#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::test_ruleset;
using aetheria::world::RegionXY;
using aetheria::worldgen::BiomeStageOutput;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::CityStageOutput;
using aetheria::worldgen::ClimateStageOutput;
using aetheria::worldgen::FeatureStageOutput;
using aetheria::worldgen::generate_cities;
using aetheria::worldgen::generate_roads;
using aetheria::worldgen::hash_stage;
using aetheria::worldgen::QuantizedElevation;
using aetheria::worldgen::RegionBuildResult;
using aetheria::worldgen::RegionGenerationConfig;
using aetheria::worldgen::RegionSlowVariables;
using aetheria::worldgen::RiverStageOutput;

[[nodiscard]] std::size_t index_of(std::uint32_t width, RegionXY tile) {
    return static_cast<std::size_t>(tile.y) * width + static_cast<std::size_t>(tile.x);
}

[[nodiscard]] std::uint64_t hash_edges(const std::vector<aetheria::rules::EdgeId>& edges) {
    auto hash = UINT64_C(14695981039346656037);
    for (const auto edge : edges) {
        auto value = aetheria::rules::value_of(edge);
        for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
            hash ^= static_cast<std::uint8_t>(value & UINT8_MAX);
            hash *= UINT64_C(1099511628211);
            value >>= 8U;
        }
    }
    return hash;
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

[[nodiscard]] aetheria::worldgen::RoadStageOutput
roads_from(const RegionBuildResult& result, const CityStageOutput& cities, bool canonicalize = true,
           const aetheria::worldgen::RoadGenerationConfig& config = {}) {
    return generate_roads(result.skeleton.elevation, result.climate, result.rivers, result.biome,
                          result.features, cities, result.skeleton.definitions, test_ruleset(),
                          UINT64_C(0x9009), config, canonicalize);
}

TEST(CityGenerationStage, TerrainBottleneckOutscoresNearbyOpenGround) {
    const auto fixture = bottleneck_fixture();
    const auto cities =
        generate_cities(fixture.elevation, fixture.climate, fixture.rivers, fixture.biome,
                        fixture.features, test_ruleset(), UINT64_C(8008), {});
    const auto choke = index_of(fixture.elevation.width, {9, 4});
    const auto plain = index_of(fixture.elevation.width, {3, 4});

    std::cout << "bottleneck choke=" << cities.bottleneck[choke]
              << " plain=" << cities.bottleneck[plain] << " choke_score=" << cities.score[choke]
              << " plain_score=" << cities.score[plain] << '\n';
    EXPECT_GT(cities.bottleneck[choke], cities.bottleneck[plain]);
    EXPECT_GT(cities.score[choke], cities.score[plain]);
}

TEST(CityGenerationStage, FullRegionScoresAll12288TilesWithinBudgetAndRespectsSpacing) {
    const auto terrain =
        build_skeleton(RegionSlowVariables{44, 128, 96}, UINT64_C(440044), test_ruleset());
    const auto start = std::chrono::steady_clock::now();
    const auto cities =
        generate_cities(terrain.skeleton.elevation, terrain.climate, terrain.rivers, terrain.biome,
                        terrain.features, test_ruleset(), UINT64_C(8008), {});
    const auto elapsed = std::chrono::steady_clock::now() - start;

    std::cout << "bottleneck_12288_ms="
              << std::chrono::duration<double, std::milli>{elapsed}.count()
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

TEST(RoadGenerationStage, CanonicalOrderIgnoresShuffledInputAndNegativeControlChangesEdges) {
    const auto terrain =
        build_skeleton(RegionSlowVariables{51, 128, 96}, UINT64_C(515151), test_ruleset());
    auto shuffled = terrain.cities;
    std::ranges::reverse(shuffled.cities);
    const auto canonical_a = roads_from(terrain, terrain.cities, true);
    const auto canonical_b = roads_from(terrain, shuffled, true);
    const auto uncanonical_a = roads_from(terrain, terrain.cities, false);
    const auto uncanonical_b = roads_from(terrain, shuffled, false);

    std::cout << "canonical_edges_hash_a=" << hash_edges(canonical_a.edges)
              << " canonical_edges_hash_b=" << hash_edges(canonical_b.edges)
              << " negative_edges_a=" << hash_edges(uncanonical_a.edges)
              << " negative_edges_b=" << hash_edges(uncanonical_b.edges) << '\n';
    EXPECT_EQ(canonical_a.edges, canonical_b.edges);
    EXPECT_EQ(hash_stage(canonical_a), hash_stage(canonical_b));
    EXPECT_NE(uncanonical_a.edges, uncanonical_b.edges);
}

TEST(RoadGenerationStage, AddsLoopsCompoundCrossingsAndSymmetricEdges) {
    const auto result =
        build_skeleton(RegionSlowVariables{7, 128, 96}, UINT64_C(20260816), test_ruleset());
    const auto& roads = result.roads;
    const auto loop_count = static_cast<std::size_t>(
        std::ranges::count_if(roads.connections, &aetheria::worldgen::RoadConnection::loop));
    ASSERT_GT(loop_count, 0U);
    EXPECT_GT(roads.connections.size(), result.cities.cities.size() - 1U);
    const auto tree_edges = result.cities.cities.size() - 1U;
    EXPECT_GE(loop_count * 100U, tree_edges * 10U);
    EXPECT_LE(loop_count * 100U, tree_edges * 20U);

    const auto loop =
        *std::ranges::find_if(roads.connections, &aetheria::worldgen::RoadConnection::loop);
    std::map<std::uint32_t, std::vector<std::uint32_t>> graph;
    bool skipped{};
    for (const auto& edge : roads.connections) {
        if (!skipped && edge == loop) {
            skipped = true;
            continue;
        }
        graph[edge.first_city].push_back(edge.second_city);
        graph[edge.second_city].push_back(edge.first_city);
    }
    std::set<std::uint32_t> visited{loop.first_city};
    std::queue<std::uint32_t> open;
    open.push(loop.first_city);
    while (!open.empty()) {
        const auto current = open.front();
        open.pop();
        for (const auto next : graph[current]) {
            if (visited.insert(next).second) {
                open.push(next);
            }
        }
    }
    EXPECT_TRUE(visited.contains(loop.second_city));

    const auto tiles = aetheria::worldgen::populate(result.skeleton, {});
    std::size_t compound{};
    for (const auto edge : roads.edges) {
        const auto* definition = test_ruleset().edge(edge);
        ASSERT_NE(definition, nullptr);
        if ((definition->flags & aetheria::rules::kEdgeRoadFlag) != 0 &&
            (definition->flags & aetheria::rules::kEdgeRiverFlag) != 0 &&
            (definition->flags & aetheria::rules::kEdgeBridgeFlag) != 0) {
            ++compound;
        }
    }
    std::cout << "road_graph cities=" << result.cities.cities.size()
              << " connections=" << roads.connections.size() << " loops=" << loop_count
              << " compound_directed_edges=" << compound << '\n';
    EXPECT_GT(compound, 0U);
    for (std::int16_t y = 0; y < static_cast<std::int16_t>(tiles.height); ++y) {
        for (std::int16_t x = 0; x < static_cast<std::int16_t>(tiles.width); ++x) {
            const RegionXY here{x, y};
            if (x + 1 < static_cast<std::int16_t>(tiles.width)) {
                const RegionXY east{static_cast<std::int16_t>(x + 1), y};
                EXPECT_EQ(tiles.edge_between(here, east), tiles.edge_between(east, here));
            }
            if (y + 1 < static_cast<std::int16_t>(tiles.height)) {
                const RegionXY south{x, static_cast<std::int16_t>(y + 1)};
                EXPECT_EQ(tiles.edge_between(here, south), tiles.edge_between(south, here));
            }
        }
    }
}

TEST(RegionGeneration, ChangingRoadParametersCannotMoveStagesOneThroughEight) {
    RegionGenerationConfig original;
    auto changed = original;
    changed.roads.loop_percent_override = 20;
    const RegionSlowVariables slow{61, 128, 96};
    const auto before = build_skeleton(slow, UINT64_C(616161), test_ruleset(), original);
    const auto after = build_skeleton(slow, UINT64_C(616161), test_ruleset(), changed);

    EXPECT_EQ(hash_stage(before.plates), hash_stage(after.plates));
    EXPECT_EQ(hash_stage(before.height), hash_stage(after.height));
    EXPECT_EQ(hash_stage(before.erosion), hash_stage(after.erosion));
    EXPECT_EQ(hash_stage(before.climate), hash_stage(after.climate));
    EXPECT_EQ(hash_stage(before.rivers), hash_stage(after.rivers));
    EXPECT_EQ(hash_stage(before.biome), hash_stage(after.biome));
    EXPECT_EQ(hash_stage(before.features), hash_stage(after.features));
    EXPECT_EQ(hash_stage(before.cities), hash_stage(after.cities));
    EXPECT_NE(hash_stage(before.roads), hash_stage(after.roads));
}

}  // namespace

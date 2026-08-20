#include "core/worldgen/civ_tiles.h"
#include "core/worldgen/gen_stage_ids.h"
#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::EdgeId;
using aetheria::tests::test_ruleset;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::derive_region_stage_seed;
using aetheria::worldgen::generate_history_from_sites;
using aetheria::worldgen::RegionBuildResult;
using aetheria::worldgen::RegionSlowVariables;

[[nodiscard]] std::uint64_t hash_edges(const std::vector<EdgeId>& edges) {
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

[[nodiscard]] std::uint64_t hash_survivors(const std::vector<std::uint8_t>& survivors) {
    auto hash = UINT64_C(14695981039346656037);
    for (const auto value : survivors) {
        hash ^= value;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

[[nodiscard]] aetheria::worldgen::HistoryStageOutput
history_from(const RegionBuildResult& terrain,
             aetheria::worldgen::CityStageOutput ancient_sites,
             bool canonicalize_city_order) {
    return generate_history_from_sites(
        terrain.skeleton.elevation, terrain.climate, terrain.rivers, terrain.biome,
        terrain.features, std::move(ancient_sites), terrain.skeleton.definitions, test_ruleset(),
        derive_region_stage_seed(UINT64_C(515151), 51, aetheria::worldgen::detail::kHistoryStageId),
        canonicalize_city_order);
}

TEST(HistoryGenerationStage, CanonicalOrderIgnoresShuffledSitesAndNegativeControlChangesEdges) {
    const auto terrain =
        build_skeleton(RegionSlowVariables{51, 128, 96}, UINT64_C(515151), test_ruleset());
    auto shuffled = terrain.history.ancient_sites;
    std::ranges::reverse(shuffled.cities);
    const auto canonical_a = history_from(terrain, terrain.history.ancient_sites, true);
    const auto canonical_b = history_from(terrain, shuffled, true);
    const auto uncanonical_a = history_from(terrain, terrain.history.ancient_sites, false);
    const auto uncanonical_b = history_from(terrain, shuffled, false);

    std::cout << "history_canonical_edges_hash_a=" << hash_edges(canonical_a.edges)
              << " history_canonical_edges_hash_b=" << hash_edges(canonical_b.edges)
              << " history_negative_edges_hash_a=" << hash_edges(uncanonical_a.edges)
              << " history_negative_edges_hash_b=" << hash_edges(uncanonical_b.edges) << '\n'
              << "cataclysm_canonical_survivor_hash_a=" << hash_survivors(canonical_a.survivor)
              << " cataclysm_canonical_survivor_hash_b=" << hash_survivors(canonical_b.survivor)
              << " cataclysm_negative_survivor_hash_a=" << hash_survivors(uncanonical_a.survivor)
              << " cataclysm_negative_survivor_hash_b=" << hash_survivors(uncanonical_b.survivor)
              << '\n';
    EXPECT_EQ(canonical_a.edges, canonical_b.edges);
    EXPECT_NE(uncanonical_a.edges, uncanonical_b.edges);
    EXPECT_EQ(canonical_a.survivor, canonical_b.survivor);
    EXPECT_NE(uncanonical_a.survivor, uncanonical_b.survivor);
}

TEST(HistoryGenerationStage, AncientRoadAndSkippedRiverMasksAreSymmetric) {
    const auto result =
        build_skeleton(RegionSlowVariables{7, 128, 96}, UINT64_C(20260820), test_ruleset());
    const auto& history = result.history;
    const auto base = aetheria::worldgen::detail::make_base_tiles(
        result.skeleton.elevation, result.climate, result.rivers, result.biome, result.features,
        result.skeleton.definitions);
    std::size_t skipped_river_edges{};
    for (std::size_t y = 0; y < history.features.height; ++y) {
        for (std::size_t x = 0; x < history.features.width; ++x) {
            const auto here = y * history.features.width + x;
            if (x + 1U < history.features.width) {
                const auto east = here + 1U;
                const auto forward = here * 4U + 1U;
                const auto backward = east * 4U + 3U;
                EXPECT_EQ(history.edges[forward], history.edges[backward]);
                EXPECT_EQ(history.skipped_river_edges[forward],
                          history.skipped_river_edges[backward]);
                if (history.skipped_river_edges[forward] != 0) {
                    ++skipped_river_edges;
                    EXPECT_EQ(history.edges[forward], base.edges[forward]);
                    EXPECT_NE(history.edges[forward],
                              test_ruleset().civilization_rules().history.road_edge);
                    const auto* edge = test_ruleset().edge(history.edges[forward]);
                    ASSERT_NE(edge, nullptr);
                    EXPECT_NE(edge->flags & aetheria::rules::kEdgeRiverFlag, 0U);
                }
            }
            if (y + 1U < history.features.height) {
                const auto south = here + history.features.width;
                const auto forward = here * 4U + 2U;
                const auto backward = south * 4U;
                EXPECT_EQ(history.edges[forward], history.edges[backward]);
                EXPECT_EQ(history.skipped_river_edges[forward],
                          history.skipped_river_edges[backward]);
                if (history.skipped_river_edges[forward] != 0) {
                    ++skipped_river_edges;
                    EXPECT_EQ(history.edges[forward], base.edges[forward]);
                    EXPECT_NE(history.edges[forward],
                              test_ruleset().civilization_rules().history.road_edge);
                    const auto* edge = test_ruleset().edge(history.edges[forward]);
                    ASSERT_NE(edge, nullptr);
                    EXPECT_NE(edge->flags & aetheria::rules::kEdgeRiverFlag, 0U);
                }
            }
        }
    }
    EXPECT_GT(skipped_river_edges, 0U);
    std::cout << "history_unique_skipped_river_edges=" << skipped_river_edges << '\n';
}

}  // namespace

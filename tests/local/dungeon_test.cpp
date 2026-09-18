#include "core/local/dungeon.h"
#include "core/serialize/normalized_state_hash.h"
#include "core/serialize/zone_codec.h"
#include "core/worldgen/region_generator.h"
#include "tests/support/performance.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/zone/zone_test_support.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <numeric>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>

#include "tests/local/dungeon_test_support.h"

namespace {

using aetheria::local::DungeonGenerated;
using aetheria::local::DungeonPersistentState;
using aetheria::rules::DungeonArchetype;
using aetheria::tests::test_ruleset;

using namespace aetheria::tests::dungeon_support;

[[nodiscard]] double correlation(const std::vector<double>& left,
                                 const std::vector<double>& right) {
    const auto left_mean = std::accumulate(left.begin(), left.end(), 0.0) / left.size();
    const auto right_mean = std::accumulate(right.begin(), right.end(), 0.0) / right.size();
    double numerator{};
    double left_square{};
    double right_square{};
    for (std::size_t index = 0; index < left.size(); ++index) {
        const auto left_delta = left[index] - left_mean;
        const auto right_delta = right[index] - right_mean;
        numerator += left_delta * right_delta;
        left_square += left_delta * left_delta;
        right_square += right_delta * right_delta;
    }
    return numerator / std::sqrt(left_square * right_square);
}

class DungeonArchetypeTest : public testing::TestWithParam<DungeonArchetype> {};

TEST_P(DungeonArchetypeTest, SharedGeneratorMakesEveryDepthStrictlyHarder) {
    const auto dungeon = generate(UINT64_C(0xC0FFEE), GetParam(), 7);
    ASSERT_TRUE(aetheria::local::valid_dungeon(dungeon, test_ruleset()));
    ASSERT_EQ(dungeon.floors.size(), 7U);
    for (std::size_t index = 1; index < dungeon.floors.size(); ++index) {
        EXPECT_GT(dungeon.floors[index].difficulty, dungeon.floors[index - 1U].difficulty);
        EXPECT_GT(dungeon.floors[index].enemy_count, dungeon.floors[index - 1U].enemy_count);
        EXPECT_GT(dungeon.floors[index].light_cost, dungeon.floors[index - 1U].light_cost);
        EXPECT_GT(dungeon.floors[index].retreat_cost, dungeon.floors[index - 1U].retreat_cost);
    }
}

INSTANTIATE_TEST_SUITE_P(NaturalArtificialHybrid, DungeonArchetypeTest,
                         testing::Values(DungeonArchetype::Natural,
                                         DungeonArchetype::Artificial,
                                         DungeonArchetype::Hybrid));

TEST(DungeonGeneration, ArchetypesAreParametersOfOneLayoutPipeline) {
    const auto natural = generate(101, DungeonArchetype::Natural);
    const auto artificial = generate(101, DungeonArchetype::Artificial);
    const auto hybrid = generate(101, DungeonArchetype::Hybrid);
    const auto room_counts = std::array{natural.floors.front().rooms.size(),
                                        artificial.floors.front().rooms.size(),
                                        hybrid.floors.front().rooms.size()};
    EXPECT_EQ(room_counts, (std::array<std::size_t, 3>{7, 8, 8}));
    const auto count_natural = [](const auto& dungeon) {
        return std::ranges::count_if(dungeon.floors.front().rooms,
                                     [](const auto& room) { return room.natural; });
    };
    EXPECT_EQ(count_natural(natural), 7);
    EXPECT_EQ(count_natural(artificial), 0);
    EXPECT_GT(count_natural(hybrid), 0);
    EXPECT_LT(count_natural(hybrid), 8);
    std::cout << "dungeon_shared_pipeline rooms=" << room_counts[0] << ',' << room_counts[1]
              << ',' << room_counts[2] << " natural_rooms=" << count_natural(natural) << ','
              << count_natural(artificial) << ',' << count_natural(hybrid) << '\n';
}

TEST(DungeonGeneration, SiteSourceOwnsDepthBoundsAndStoryUsesExplicitData) {
    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        EXPECT_GE(aetheria::local::choose_dungeon_depth(
                      aetheria::local::DungeonDepthSource::BeastLair, seed),
                  1U);
        EXPECT_LE(aetheria::local::choose_dungeon_depth(
                      aetheria::local::DungeonDepthSource::BeastLair, seed),
                  2U);
        EXPECT_GE(aetheria::local::choose_dungeon_depth(
                      aetheria::local::DungeonDepthSource::Mine, seed),
                  3U);
        EXPECT_LE(aetheria::local::choose_dungeon_depth(
                      aetheria::local::DungeonDepthSource::Mine, seed),
                  6U);
        EXPECT_GE(aetheria::local::choose_dungeon_depth(
                      aetheria::local::DungeonDepthSource::Ruin, seed),
                  5U);
        EXPECT_LE(aetheria::local::choose_dungeon_depth(
                      aetheria::local::DungeonDepthSource::Ruin, seed),
                  10U);
    }
    EXPECT_EQ(aetheria::local::choose_dungeon_depth(
                  aetheria::local::DungeonDepthSource::Story, 0, 13),
              13U);
}

TEST(DungeonGeneration, EntranceClueCorrelatesWithActualDeepestDifficultyAcrossOneHundred) {
    std::vector<double> clues;
    std::vector<double> actual;
    for (std::uint64_t index = 0; index < 100U; ++index) {
        const auto depth = aetheria::local::choose_dungeon_depth(
            aetheria::local::DungeonDepthSource::Ruin, index * 17U + 3U);
        const auto dungeon = generate(index * UINT64_C(0x9E3779B97F4A7C15),
                                      static_cast<DungeonArchetype>(index % 3U), depth);
        clues.push_back(dungeon.entrance_clue.predicted_deepest_difficulty);
        actual.push_back(dungeon.floors.back().difficulty);
    }
    const auto measured = correlation(clues, actual);
    EXPECT_GT(measured, 0.95);
    std::cout << "dungeon_clue_correlation n=100 pearson=" << measured << '\n';
}

TEST(DungeonGeneration, SameSeedIsDeterministicAndMinimumOfFiveIsUnderBudget) {
    const auto first = generate(UINT64_C(0x515151), DungeonArchetype::Hybrid, 10);
    const auto repeated = generate(UINT64_C(0x515151), DungeonArchetype::Hybrid, 10);
    EXPECT_EQ(aetheria::local::hash_dungeon(first), aetheria::local::hash_dungeon(repeated));
    const auto minimum = aetheria::tests::minimum_milliseconds_after_warmup([&] {
        const auto start = std::chrono::steady_clock::now();
        const auto generated = generate(UINT64_C(0x515151), DungeonArchetype::Hybrid, 10);
        EXPECT_EQ(generated.floors.size(), 10U);
        return std::chrono::duration<double, std::milli>{std::chrono::steady_clock::now() - start}
            .count();
    });
    EXPECT_LT(minimum, 10.0);
    std::cout << "dungeon_deterministic_hash=" << aetheria::local::hash_dungeon(first)
              << " min_of_5_ms=" << minimum << " floors=" << first.floors.size() << '\n';
}

}  // namespace

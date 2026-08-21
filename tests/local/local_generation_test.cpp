#include "tests/local/local_test_support.h"
#include "tests/support/performance.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <type_traits>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::kLocalCenter;
using aetheria::tests::kLocalSiteSeed;
using aetheria::tests::open_site_layer;
using aetheria::tests::set_site_edge;
using aetheria::tests::test_ruleset;

static_assert(!std::is_invocable_v<decltype(&aetheria::local::build_open_local_skeleton),
                                   const aetheria::local::LocalFastVars&, std::uint64_t,
                                   const aetheria::rules::Ruleset&>);

TEST(LocalProjection, SeedFormulaAndSlowSignatureAreDeterministic) {
    const auto expected = aetheria::worldgen::splitmix64(
        kLocalSiteSeed ^ ((static_cast<std::uint64_t>(kLocalCenter.y) << 16U) |
                          static_cast<std::uint64_t>(kLocalCenter.x)));
    EXPECT_EQ(aetheria::local::derive_local_seed(kLocalSiteSeed, kLocalCenter.x,
                                                 kLocalCenter.y),
              expected);
    EXPECT_NE(aetheria::local::derive_local_seed(kLocalSiteSeed, kLocalCenter.x + 1U,
                                                 kLocalCenter.y),
              expected);
}

TEST(LocalGeneration, RouteBIsDeterministicNonEmptyAndUnderBudget) {
    auto parent = open_site_layer();
    const auto river = *test_ruleset().find_edge("edge.river");
    const auto road = *test_ruleset().find_edge("edge.highway");
    set_site_edge(parent, {31, 32}, kLocalCenter, river);
    set_site_edge(parent, kLocalCenter, {33, 32}, road);
    const auto feature = *test_ruleset().find_feature("feature.forest");
    const auto slow = aetheria::local::project_local_slow_vars(
        parent, kLocalCenter, kLocalSiteSeed, feature, test_ruleset());
    const auto seed = aetheria::local::derive_local_seed(
        kLocalSiteSeed, kLocalCenter.x, kLocalCenter.y);
    const auto first =
        aetheria::local::build_open_local_skeleton(slow, seed, test_ruleset());
    const auto second =
        aetheria::local::build_open_local_skeleton(slow, seed, test_ruleset());
    const auto changed =
        aetheria::local::build_open_local_skeleton(slow, seed + 1U, test_ruleset());
    const auto first_hash = aetheria::local::hash_open_local_skeleton(first);
    const auto second_hash = aetheria::local::hash_open_local_skeleton(second);
    const auto changed_hash = aetheria::local::hash_open_local_skeleton(changed);
    EXPECT_EQ(first_hash, second_hash);
    EXPECT_NE(first_hash, changed_hash);
    EXPECT_EQ(first.tiles.ground.size(), aetheria::local::kLocalTileCount);
    EXPECT_GT(first.road_path_count, 0U);
    EXPECT_GT(first.river_path_count, 0U);
    EXPECT_GT(first.scatter_count, 0U);
    EXPECT_GT(first.object_count, 0U);
    EXPECT_EQ(std::ranges::count_if(first.tiles.occupant,
                                    [](auto occupant) { return occupant != 0; }),
              0);

    const auto minimum_milliseconds =
        aetheria::tests::minimum_milliseconds_after_warmup([&] {
            const auto start = std::chrono::steady_clock::now();
            const auto generated =
                aetheria::local::build_open_local_skeleton(slow, seed, test_ruleset());
            EXPECT_EQ(generated.tiles.ground.size(), aetheria::local::kLocalTileCount);
            return std::chrono::duration<double, std::milli>{
                       std::chrono::steady_clock::now() - start}
                .count();
        });
    std::cout << "local_route_b_min_of_5_ms=" << minimum_milliseconds
              << " generated_tiles=" << first.tiles.ground.size()
              << " road_paths=" << first.road_path_count
              << " river_paths=" << first.river_path_count
              << " scatter=" << first.scatter_count << " objects=" << first.object_count
              << " occupants="
              << std::ranges::count_if(first.tiles.occupant,
                                       [](auto occupant) { return occupant != 0; })
              << " normalized_hash=" << first_hash << " changed_seed_hash=" << changed_hash
              << '\n';
    EXPECT_LT(minimum_milliseconds, 10.0);
}

}  // namespace

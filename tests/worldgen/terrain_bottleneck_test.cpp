#include "core/worldgen/city_scoring.h"
#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

namespace {

[[nodiscard]] std::int64_t tile_cost(const aetheria::worldgen::RegionBuildResult& result,
                                     std::size_t index) {
    const auto& ruleset = aetheria::tests::test_ruleset();
    const auto* terrain = ruleset.terrain(result.biome.terrain[index]);
    const auto* relief = ruleset.relief(result.biome.relief[index]);
    const auto* feature = ruleset.feature(result.history.features.feature[index]);
    if (terrain == nullptr || relief == nullptr || feature == nullptr) {
        return std::numeric_limits<std::int64_t>::max();
    }
    return static_cast<std::int64_t>(terrain->move_cost) + relief->move_cost + feature->move_cost;
}

TEST(TerrainBottleneck, FixedRegionContainsNewlyRecognizedMountainPass) {
    constexpr auto seed = UINT64_C(12345);
    const auto& ruleset = aetheria::tests::test_ruleset();
    const auto result = aetheria::worldgen::build_skeleton(
        aetheria::worldgen::RegionSlowVariables{0, 128, 96}, seed, ruleset);
    const auto& elevation = result.skeleton.elevation;
    const auto& scores = result.cities.bottleneck;
    const auto radius = ruleset.civilization_rules().bottleneck_radius;
    const auto barrier = ruleset.civilization_rules().bottleneck_barrier_move_cost;
    const std::vector<std::uint8_t> land_only = elevation.land;
    std::size_t selected = scores.size();
    std::size_t selected_barriers{};
    std::uint16_t selected_old{};
    for (std::size_t index = 0; index < scores.size(); ++index) {
        const auto old_score = aetheria::worldgen::detail::local_bottleneck_score(
            land_only, elevation.width, elevation.height, index, radius);
        if (scores[index] <= old_score) {
            continue;
        }
        const auto x = index % elevation.width;
        const auto y = index / elevation.width;
        if (x < radius || y < radius || x + radius >= elevation.width ||
            y + radius >= elevation.height) {
            continue;
        }
        const std::array neighbors{y > 0 ? index - elevation.width : scores.size(),
                                   x + 1U < elevation.width ? index + 1U : scores.size(),
                                   y + 1U < elevation.height ? index + elevation.width
                                                              : scores.size(),
                                   x > 0 ? index - 1U : scores.size()};
        std::size_t land_barriers{};
        for (const auto neighbor : neighbors) {
            land_barriers += neighbor < scores.size() && elevation.land[neighbor] != 0 &&
                             tile_cost(result, neighbor) >= barrier;
        }
        if (land_barriers > selected_barriers) {
            selected = index;
            selected_barriers = land_barriers;
            selected_old = old_score;
        }
    }

    ASSERT_LT(selected, scores.size());
    const auto x = selected % elevation.width;
    const auto y = selected / elevation.width;
    const std::array neighbors{y > 0 ? selected - elevation.width : scores.size(),
                               x + 1U < elevation.width ? selected + 1U : scores.size(),
                               y + 1U < elevation.height ? selected + elevation.width
                                                          : scores.size(),
                               x > 0 ? selected - 1U : scores.size()};
    std::cout << "real_mountain_pass seed=" << seed << " coordinate=(" << x << ',' << y
              << ") score=" << scores[selected] << " land_only_score=" << selected_old
              << " adjacent_scores=";
    for (const auto neighbor : neighbors) {
        std::cout << (neighbor < scores.size() ? scores[neighbor] : 0) << ',';
    }
    std::cout << " adjacent_costs=";
    for (const auto neighbor : neighbors) {
        std::cout << (neighbor < scores.size() ? tile_cost(result, neighbor) : 0) << ',';
    }
    std::cout << '\n';
    EXPECT_GT(selected_barriers, 0U);
    EXPECT_GT(scores[selected], selected_old);
}

}  // namespace

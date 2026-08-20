#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::Ruleset;
using aetheria::tests::test_ruleset;
using aetheria::world::FactionId;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::RegionSlowVariables;

[[nodiscard]] std::int64_t terrain_cost(const aetheria::world::RegionTiles& tiles,
                                        std::size_t index, const Ruleset& ruleset) {
    const auto* terrain = ruleset.terrain(tiles.base.at(index));
    const auto* relief = ruleset.relief(tiles.relief.at(index));
    const auto* feature = ruleset.feature(tiles.feature.at(index));
    if (terrain == nullptr || relief == nullptr || feature == nullptr) {
        throw std::runtime_error{"remeasurement encountered invalid def"};
    }
    return static_cast<std::int64_t>(terrain->move_cost) + relief->move_cost + feature->move_cost;
}

[[nodiscard]] std::int64_t
farmland_contribution(const aetheria::worldgen::RegionBuildResult& result,
                      const aetheria::worldgen::CitySite& city, const Ruleset& ruleset) {
    const auto width = result.skeleton.elevation.width;
    const auto height = result.skeleton.elevation.height;
    std::int64_t farmland{};
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            const auto x = static_cast<int>(city.tile.x) + dx;
            const auto y = static_cast<int>(city.tile.y) + dy;
            if (x < 0 || y < 0 || x >= static_cast<int>(width) || y >= static_cast<int>(height)) {
                continue;
            }
            const auto index = static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x);
            const auto* terrain = ruleset.terrain(result.biome.terrain[index]);
            const auto* relief = ruleset.relief(result.biome.relief[index]);
            if (result.skeleton.elevation.land[index] != 0 && terrain != nullptr &&
                relief != nullptr && terrain->yield.food >= 2 && relief->move_cost <= 2) {
                ++farmland;
            }
        }
    }
    return farmland * ruleset.civilization_rules().scoring_weights.farmland;
}

TEST(WorldgenRemeasurement, ReportsBoundaryCityAndBottleneckMetricsAtFixedSeed) {
    constexpr auto seed = UINT64_C(12345);
    const auto& ruleset = test_ruleset();
    const auto result = build_skeleton(RegionSlowVariables{0, 128, 96}, seed, ruleset);
    const auto tiles = aetheria::worldgen::populate(result.skeleton, {});
    const auto hills = *ruleset.find_relief("relief.hills");
    const auto mountain = *ruleset.find_relief("relief.mountain");
    const auto forest = *ruleset.find_feature("feature.forest");

    std::size_t land{};
    std::size_t land_hills{};
    std::size_t land_mountains{};
    std::size_t land_forests{};
    std::int64_t map_cost{};
    std::size_t boundary_tiles{};
    std::size_t boundary_hills{};
    std::size_t boundary_mountains{};
    std::size_t boundary_forests{};
    std::int64_t boundary_cost{};
    for (std::size_t index = 0; index < tiles.tile_count(); ++index) {
        const auto* terrain = ruleset.terrain(tiles.base[index]);
        ASSERT_NE(terrain, nullptr);
        if ((terrain->flags & aetheria::rules::kTerrainWaterFlag) != 0) {
            continue;
        }
        ++land;
        land_hills += tiles.relief[index] == hills;
        land_mountains += tiles.relief[index] == mountain;
        land_forests += tiles.feature[index] == forest;
        const auto cost = terrain_cost(tiles, index, ruleset);
        map_cost += cost;
        const auto x = index % tiles.width;
        const auto y = index / tiles.width;
        const std::array neighbors{y > 0 ? index - tiles.width : tiles.tile_count(),
                                   x + 1U < tiles.width ? index + 1U : tiles.tile_count(),
                                   y + 1U < tiles.height ? index + tiles.width : tiles.tile_count(),
                                   x > 0 ? index - 1U : tiles.tile_count()};
        const bool faction_boundary = std::ranges::any_of(neighbors, [&](std::size_t neighbor) {
            return neighbor < tiles.tile_count() && tiles.owner[index] != FactionId{0} &&
                   tiles.owner[neighbor] != FactionId{0} &&
                   tiles.owner[index] != tiles.owner[neighbor];
        });
        if (faction_boundary) {
            ++boundary_tiles;
            boundary_hills += tiles.relief[index] == hills;
            boundary_mountains += tiles.relief[index] == mountain;
            boundary_forests += tiles.feature[index] == forest;
            boundary_cost += cost;
        }
    }

    ASSERT_EQ(result.cities.cities.size(), 18U);
    std::vector<std::int32_t> city_scores;
    std::vector<std::int64_t> farmland_points;
    std::vector<double> farmland_shares;
    for (const auto& city : result.cities.cities) {
        const auto contribution = farmland_contribution(result, city, ruleset);
        city_scores.push_back(city.score);
        farmland_points.push_back(contribution);
        farmland_shares.push_back(static_cast<double>(contribution) * 100.0 / city.score);
    }
    std::ranges::sort(city_scores);
    std::ranges::sort(farmland_points);
    std::ranges::sort(farmland_shares);
    const auto median_score =
        (static_cast<double>(city_scores[8]) + static_cast<double>(city_scores[9])) / 2.0;
    const auto bottleneck_nonzero =
        std::ranges::count_if(result.cities.bottleneck, [](auto score) { return score != 0; });

    ASSERT_GT(land, 0U);
    ASSERT_GT(boundary_tiles, 0U);
    std::cout << "remeasure seed=" << seed << " land_tiles=" << land
              << " faction_boundary_tiles=" << boundary_tiles
              << " faction_boundary_avg=" << static_cast<double>(boundary_cost) / boundary_tiles
              << " map_avg=" << static_cast<double>(map_cost) / land
              << " boundary_hills=" << boundary_hills
              << " boundary_mountains=" << boundary_mountains
              << " boundary_forests=" << boundary_forests << " land_hills=" << land_hills
              << " land_mountains=" << land_mountains << " land_forests=" << land_forests
              << " city_score_min=" << city_scores.front() << " city_score_median=" << median_score
              << " city_score_max=" << city_scores.back()
              << " farmland_points_min=" << farmland_points.front()
              << " farmland_points_max=" << farmland_points.back()
              << " farmland_share_min=" << farmland_shares.front()
              << " farmland_share_max=" << farmland_shares.back()
              << " bottleneck_nonzero=" << bottleneck_nonzero << '\n';
}

}  // namespace

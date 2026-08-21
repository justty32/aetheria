#include "core/worldgen/region_generator.h"
#include "tests/support/performance.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/worldgen/influence_test_support.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::owner_hash;
using aetheria::tests::test_ruleset;
using aetheria::world::FactionId;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::populate;
using aetheria::worldgen::RegionSlowVariables;

[[nodiscard]] std::int64_t terrain_cost(const aetheria::world::RegionTiles& tiles,
                                        std::size_t index) {
    const auto* terrain = test_ruleset().terrain(tiles.base.at(index));
    const auto* relief = test_ruleset().relief(tiles.relief.at(index));
    const auto* feature = test_ruleset().feature(tiles.feature.at(index));
    if (terrain == nullptr || relief == nullptr || feature == nullptr) {
        throw std::runtime_error{"faction metric invalid def"};
    }
    return static_cast<std::int64_t>(terrain->move_cost) + relief->move_cost +
           feature->move_cost;
}

TEST(FactionGenerationStage, RealRegionIsCanonicalDistributedAndMeasured) {
    constexpr auto seed = UINT64_C(12345);
    const auto result = build_skeleton(RegionSlowVariables{0, 128, 96}, seed, test_ruleset());
    const auto tiles = populate(result.skeleton, {});
    auto capitals = result.factions.capitals;
    std::ranges::reverse(capitals);
    aetheria::worldgen::InfluenceSpreadDiagnostics diagnostics;
    auto global_claims = aetheria::worldgen::claim_all_land(
        tiles, capitals, test_ruleset(),
        test_ruleset().civilization_rules().factions.influence_season);
    auto released = aetheria::worldgen::release_beyond_governance(
        global_claims,
        test_ruleset().civilization_rules().factions.governance_max_cost);
    auto shuffled = aetheria::worldgen::spread_influence(
        tiles, capitals, test_ruleset(), test_ruleset().civilization_rules().factions,
        &diagnostics);
    const auto minimum_milliseconds = aetheria::tests::minimum_milliseconds_after_warmup([&] {
        diagnostics = {};
        const auto start = std::chrono::steady_clock::now();
        global_claims = aetheria::worldgen::claim_all_land(
            tiles, capitals, test_ruleset(),
            test_ruleset().civilization_rules().factions.influence_season);
        released = aetheria::worldgen::release_beyond_governance(
            global_claims,
            test_ruleset().civilization_rules().factions.governance_max_cost);
        shuffled = aetheria::worldgen::spread_influence(
            tiles, capitals, test_ruleset(), test_ruleset().civilization_rules().factions,
            &diagnostics);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        return std::chrono::duration<double, std::milli>{elapsed}.count();
    });
    EXPECT_EQ(shuffled, result.factions.owner);
    EXPECT_EQ(released, result.factions.owner);

    auto negative_capitals = result.factions.capitals;
    ASSERT_GE(negative_capitals.size(), 2U);
    std::swap(negative_capitals[0].faction, negative_capitals[1].faction);
    const auto negative = aetheria::worldgen::spread_influence(
        tiles, negative_capitals, test_ruleset(), test_ruleset().civilization_rules().factions);
    EXPECT_NE(negative, result.factions.owner);

    std::size_t unowned{};
    std::size_t unowned_land{};
    std::size_t land{};
    std::int64_t map_cost{};
    std::int64_t faction_boundary_cost{};
    std::size_t faction_boundary_tiles{};
    std::int64_t global_boundary_cost{};
    std::size_t global_boundary_tiles{};
    std::size_t global_unowned_land{};
    for (std::size_t index = 0; index < tiles.tile_count(); ++index) {
        const auto* terrain = test_ruleset().terrain(tiles.base[index]);
        ASSERT_NE(terrain, nullptr);
        const bool is_land = (terrain->flags & aetheria::rules::kTerrainWaterFlag) == 0;
        unowned += tiles.owner[index] == FactionId{0};
        if (!is_land) {
            continue;
        }
        ++land;
        unowned_land += tiles.owner[index] == FactionId{0};
        global_unowned_land += global_claims.owner[index] == FactionId{0};
        const auto cost = terrain_cost(tiles, index);
        map_cost += cost;
        const auto x = index % tiles.width;
        const auto y = index / tiles.width;
        const std::array neighbors{
            y > 0 ? index - tiles.width : tiles.tile_count(),
            x + 1U < tiles.width ? index + 1U : tiles.tile_count(),
            y + 1U < tiles.height ? index + tiles.width : tiles.tile_count(),
            x > 0 ? index - 1U : tiles.tile_count()};
        const auto is_boundary = [&](const auto& owners) {
            return std::ranges::any_of(neighbors, [&](std::size_t neighbor) {
                return neighbor < tiles.tile_count() && owners[index] != FactionId{0} &&
                       owners[neighbor] != FactionId{0} && owners[index] != owners[neighbor];
            });
        };
        if (is_boundary(tiles.owner)) {
            faction_boundary_cost += cost;
            ++faction_boundary_tiles;
        }
        if (is_boundary(global_claims.owner)) {
            global_boundary_cost += cost;
            ++global_boundary_tiles;
        }
    }
    ASSERT_GT(land, 0U);
    EXPECT_GT(unowned, 0U);
    EXPECT_LT(unowned, tiles.tile_count());
    EXPECT_GT(unowned_land * 100U, land);
    EXPECT_LT(unowned_land * 100U, land * 50U);
    EXPECT_EQ(global_unowned_land, 0U);
    ASSERT_GT(global_boundary_tiles, 0U);
    EXPECT_GT(faction_boundary_tiles, 0U);
    ASSERT_LE(faction_boundary_tiles, global_boundary_tiles);
    EXPECT_GT(global_boundary_tiles - faction_boundary_tiles, 0U);
    EXPECT_GE(faction_boundary_tiles * 100U, global_boundary_tiles * 25U);
    EXPECT_LE(faction_boundary_cost, global_boundary_cost);
    EXPECT_GT(faction_boundary_cost * static_cast<std::int64_t>(land) * 100,
              map_cost * static_cast<std::int64_t>(faction_boundary_tiles) * 105);

    std::uint32_t minimum_distance{std::numeric_limits<std::uint32_t>::max()};
    for (std::size_t lhs = 0; lhs < result.factions.capitals.size(); ++lhs) {
        for (std::size_t rhs = lhs + 1U; rhs < result.factions.capitals.size(); ++rhs) {
            const auto& a = result.factions.capitals[lhs].tile;
            const auto& b = result.factions.capitals[rhs].tile;
            minimum_distance = std::min(
                minimum_distance,
                static_cast<std::uint32_t>(std::abs(static_cast<int>(a.x) - b.x) +
                                           std::abs(static_cast<int>(a.y) - b.y)));
        }
    }
    std::cout << "real_influence owner_hash=" << owner_hash(result.factions.owner)
              << " shuffled_hash=" << owner_hash(shuffled)
              << " negative_hash=" << owner_hash(negative)
              << " min_of_5_ms=" << minimum_milliseconds
              << " unowned=" << unowned << '/' << tiles.tile_count()
              << " unowned_land=" << unowned_land << '/' << land
              << " global_unowned_land=" << global_unowned_land << '/' << land
              << " global_boundary_tiles=" << global_boundary_tiles
              << " global_boundary_avg="
              << static_cast<double>(global_boundary_cost) / global_boundary_tiles
              << " post_release_boundary_tiles=" << faction_boundary_tiles
              << " post_release_boundary_avg="
              << static_cast<double>(faction_boundary_cost) / faction_boundary_tiles
              << " map_avg=" << static_cast<double>(map_cost) / land
              << " capital_min_manhattan=" << minimum_distance
              << " queue_pushes=" << diagnostics.queue_pushes
              << " stale_pops=" << diagnostics.stale_pops
              << " tie_relabels=" << diagnostics.tie_relabels
              << " max_updates_per_tile=" << diagnostics.maximum_updates_per_tile << '\n';
    EXPECT_GT(minimum_distance, 0U);
}

}  // namespace

#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/worldgen/influence_test_support.h"
#include "tests/worldgen/worldgen_test_support.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::Ruleset;
using aetheria::rules::RulesetLoader;
using aetheria::tests::copy_data_files;
using aetheria::tests::owner_hash;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::tests::write_text;
using aetheria::world::FactionId;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::hash_stage;
using aetheria::worldgen::populate;
using aetheria::worldgen::RegionGenerationConfig;
using aetheria::worldgen::RegionSlowVariables;

[[nodiscard]] Ruleset ruleset_with_influence_cost(std::int64_t cost) {
    TemporaryDirectory directory;
    copy_data_files(directory.path());
    const auto path = directory.path() / "civilization.toml";
    std::ifstream input{path};
    if (!input.is_open()) {
        throw std::runtime_error{"faction fixture open failed"};
    }
    std::string text{std::istreambuf_iterator<char>{input},
                     std::istreambuf_iterator<char>{}};
    const auto position = text.find("influence_max_cost = 100");
    if (position == std::string::npos) {
        throw std::runtime_error{"faction fixture field missing"};
    }
    text.replace(position, std::string_view{"influence_max_cost = 100"}.size(),
                 "influence_max_cost = " + std::to_string(cost));
    write_text(path, text);
    return RulesetLoader::load(directory.path());
}

[[nodiscard]] std::int64_t terrain_cost(const aetheria::world::RegionTiles& tiles,
                                        std::size_t index, const Ruleset& ruleset) {
    const auto* terrain = ruleset.terrain(tiles.base.at(index));
    const auto* relief = ruleset.relief(tiles.relief.at(index));
    const auto* feature = ruleset.feature(tiles.feature.at(index));
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
    const auto start = std::chrono::steady_clock::now();
    const auto shuffled = aetheria::worldgen::spread_influence(
        tiles, capitals, test_ruleset(), test_ruleset().civilization_rules().factions,
        &diagnostics);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_EQ(shuffled, result.factions.owner);

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
    std::int64_t boundary_cost{};
    std::size_t boundary_tiles{};
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
        const auto cost = terrain_cost(tiles, index, test_ruleset());
        map_cost += cost;
        const auto x = index % tiles.width;
        const auto y = index / tiles.width;
        const std::array neighbors{
            y > 0 ? index - tiles.width : tiles.tile_count(),
            x + 1U < tiles.width ? index + 1U : tiles.tile_count(),
            y + 1U < tiles.height ? index + tiles.width : tiles.tile_count(),
            x > 0 ? index - 1U : tiles.tile_count()};
        const bool boundary = std::ranges::any_of(neighbors, [&](std::size_t neighbor) {
            return neighbor < tiles.tile_count() && tiles.owner[index] != FactionId{0} &&
                   tiles.owner[neighbor] != FactionId{0} &&
                   tiles.owner[index] != tiles.owner[neighbor];
        });
        if (boundary) {
            boundary_cost += cost;
            ++boundary_tiles;
        }
    }
    ASSERT_GT(land, 0U);
    ASSERT_GT(boundary_tiles, 0U);
    EXPECT_GT(unowned, 0U);
    EXPECT_LT(unowned, tiles.tile_count());

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
              << " elapsed_ms="
              << std::chrono::duration<double, std::milli>{elapsed}.count()
              << " unowned=" << unowned << '/' << tiles.tile_count()
              << " unowned_land=" << unowned_land << '/' << land
              << " boundary_avg=" << static_cast<double>(boundary_cost) / boundary_tiles
              << " map_avg=" << static_cast<double>(map_cost) / land
              << " capital_min_manhattan=" << minimum_distance
              << " queue_pushes=" << diagnostics.queue_pushes
              << " stale_pops=" << diagnostics.stale_pops
              << " tie_relabels=" << diagnostics.tie_relabels
              << " max_updates_per_tile=" << diagnostics.maximum_updates_per_tile << '\n';
    EXPECT_GT(minimum_distance, 0U);
}

TEST(FactionGenerationStage, FactionDataAndConfigAreIsolatedToStagesTwelveAndLater) {
    constexpr RegionSlowVariables slow{0, 128, 96};
    constexpr auto seed = UINT64_C(12345);
    const auto before = build_skeleton(slow, seed, test_ruleset());
    const auto changed_rules = ruleset_with_influence_cost(20);
    const auto after = build_skeleton(slow, seed, changed_rules);
    EXPECT_EQ(hash_stage(before.roads), hash_stage(after.roads));
    EXPECT_EQ(hash_stage(before.portals), hash_stage(after.portals));
    EXPECT_NE(hash_stage(before.factions), hash_stage(after.factions));

    RegionGenerationConfig original;
    auto portal_config = original;
    auto faction_config = original;
    portal_config.portals.road_tier = 1;
    faction_config.factions.first_faction_id = 7;
    const auto base_groups = aetheria::worldgen::generation_parameter_hashes(original);
    const auto portal_groups = aetheria::worldgen::generation_parameter_hashes(portal_config);
    const auto faction_groups = aetheria::worldgen::generation_parameter_hashes(faction_config);
    for (std::size_t index = 0; index < base_groups.groups.size(); ++index) {
        EXPECT_EQ(base_groups.groups[index] != portal_groups.groups[index], index == 10U);
        EXPECT_EQ(base_groups.groups[index] != faction_groups.groups[index], index == 11U);
    }
}

}  // namespace

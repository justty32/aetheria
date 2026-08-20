#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/worldgen/worldgen_test_support.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::Ruleset;
using aetheria::rules::RulesetLoader;
using aetheria::rules::WorldConnectionType;
using aetheria::tests::copy_data_files;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::tests::write_text;
using aetheria::world::RegionXY;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::hash_stage;
using aetheria::worldgen::populate;
using aetheria::worldgen::RegionGenerationConfig;
using aetheria::worldgen::RegionSlowVariables;

[[nodiscard]] Ruleset ruleset_with_reversed_connections() {
    TemporaryDirectory directory;
    copy_data_files(directory.path());
    const auto path = directory.path() / "world_graph.toml";
    std::ifstream input{path};
    if (!input.is_open()) {
        throw std::runtime_error{"portal fixture cannot open world_graph.toml"};
    }
    const std::string text{std::istreambuf_iterator<char>{input},
                           std::istreambuf_iterator<char>{}};
    constexpr std::string_view marker{"[[connections]]"};
    const auto first = text.find(marker);
    if (first == std::string::npos) {
        throw std::runtime_error{"portal fixture cannot find connection blocks"};
    }
    std::vector<std::string> blocks;
    for (auto begin = first; begin != std::string::npos;) {
        const auto next = text.find(marker, begin + marker.size());
        blocks.push_back(text.substr(begin, next - begin));
        begin = next;
    }
    std::ranges::reverse(blocks);
    auto reordered = text.substr(0, first);
    for (const auto& block : blocks) {
        reordered += block;
    }
    write_text(path, reordered);
    return RulesetLoader::load(directory.path());
}

[[nodiscard]] bool water(const aetheria::world::RegionTiles& tiles, std::size_t index,
                         const Ruleset& ruleset) {
    const auto* terrain = ruleset.terrain(tiles.base.at(index));
    return terrain != nullptr &&
           (terrain->flags & aetheria::rules::kTerrainWaterFlag) != 0;
}

[[nodiscard]] bool coastal(const aetheria::world::RegionTiles& tiles, RegionXY tile,
                           const Ruleset& ruleset) {
    constexpr std::array directions{RegionXY{0, -1}, RegionXY{1, 0}, RegionXY{0, 1},
                                    RegionXY{-1, 0}};
    return std::ranges::any_of(directions, [&](RegionXY direction) {
        const RegionXY neighbor{static_cast<std::int16_t>(tile.x + direction.x),
                                static_cast<std::int16_t>(tile.y + direction.y)};
        return neighbor.x >= 0 && neighbor.y >= 0 &&
               static_cast<std::uint32_t>(neighbor.x) < tiles.width &&
               static_cast<std::uint32_t>(neighbor.y) < tiles.height &&
               water(tiles, tiles.index_of(neighbor), ruleset);
    });
}

[[nodiscard]] bool road_reaches_city(const aetheria::world::RegionTiles& tiles,
                                     RegionXY start, const Ruleset& ruleset) {
    const auto start_index = tiles.index_of(start);
    std::vector<std::uint8_t> visited(tiles.tile_count());
    std::queue<std::size_t> open;
    visited[start_index] = 1;
    open.push(start_index);
    while (!open.empty()) {
        const auto current = open.front();
        open.pop();
        if (tiles.settlement[current] != aetheria::world::SettlementTier::None) {
            return true;
        }
        const auto x = current % tiles.width;
        const auto y = current / tiles.width;
        const std::array neighbors{
            y > 0 ? current - tiles.width : tiles.tile_count(),
            x + 1U < tiles.width ? current + 1U : tiles.tile_count(),
            y + 1U < tiles.height ? current + tiles.width : tiles.tile_count(),
            x > 0 ? current - 1U : tiles.tile_count()};
        for (const auto next : neighbors) {
            if (next >= tiles.tile_count() || visited[next] != 0) {
                continue;
            }
            const auto edge = tiles.edge_between(
                {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)},
                {static_cast<std::int16_t>(next % tiles.width),
                 static_cast<std::int16_t>(next / tiles.width)});
            const auto* definition = ruleset.edge(edge);
            if (definition != nullptr &&
                (definition->flags & aetheria::rules::kEdgeRoadFlag) != 0) {
                visited[next] = 1;
                open.push(next);
            }
        }
    }
    return false;
}

TEST(PortalGenerationStage, RealRegionResolvesTypesAndEveryPortalHasRoadToCity) {
    const auto result = build_skeleton(RegionSlowVariables{0, 128, 96}, UINT64_C(12345),
                                       test_ruleset());
    const auto tiles = populate(result.skeleton, {});
    ASSERT_EQ(result.portals.portals.size(), 4U);
    for (const auto& portal : result.portals.portals) {
        const auto connection = std::ranges::find(test_ruleset().world_connections(),
                                                  portal.channel,
                                                  &aetheria::rules::WorldGraphConnection::id);
        ASSERT_NE(connection, test_ruleset().world_connections().end());
        const bool boundary = portal.tile.x == 0 || portal.tile.y == 0 ||
                              static_cast<std::uint32_t>(portal.tile.x) + 1U == tiles.width ||
                              static_cast<std::uint32_t>(portal.tile.y) + 1U == tiles.height;
        EXPECT_TRUE(road_reaches_city(tiles, portal.tile, test_ruleset()));
        if (connection->type == WorldConnectionType::SeaRoute) {
            EXPECT_TRUE(coastal(tiles, portal.tile, test_ruleset()));
        } else if (connection->type == WorldConnectionType::MountainPass ||
                   connection->type == WorldConnectionType::Underground) {
            EXPECT_TRUE(boundary);
        } else {
            EXPECT_EQ(tiles.feature[tiles.index_of(portal.tile)],
                      *test_ruleset().find_feature("feature.landmark"));
        }
        std::cout << "portal channel=" << aetheria::rules::value_of(portal.channel)
                  << " type=" << static_cast<unsigned>(connection->type)
                  << " tile=" << portal.tile.x << ',' << portal.tile.y
                  << " boundary=" << boundary << " road_to_city=true\n";
    }
}

TEST(PortalGenerationStage, DeclarationShuffleIsBitIdenticalAndStageElevenIsIsolated) {
    constexpr RegionSlowVariables slow{0, 128, 96};
    constexpr auto seed = UINT64_C(12345);
    const auto shuffled_rules = ruleset_with_reversed_connections();
    const auto canonical = build_skeleton(slow, seed, test_ruleset());
    const auto shuffled = build_skeleton(slow, seed, shuffled_rules);
    EXPECT_EQ(hash_stage(canonical.portals), hash_stage(shuffled.portals));

    RegionGenerationConfig changed;
    changed.portals.road_tier = 1;
    const auto changed_result = build_skeleton(slow, seed, test_ruleset(), changed);
    EXPECT_EQ(hash_stage(canonical.roads), hash_stage(changed_result.roads));
    EXPECT_NE(hash_stage(canonical.portals), hash_stage(changed_result.portals));
    std::cout << "portal_order_hash=" << hash_stage(canonical.portals)
              << " shuffled_hash=" << hash_stage(shuffled.portals)
              << " stage10_hash=" << hash_stage(canonical.roads) << '\n';
}

}  // namespace

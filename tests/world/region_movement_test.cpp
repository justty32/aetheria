#include "core/serialize/normalized_state_hash.h"
#include "core/serialize/zone_codec.h"
#include "core/world/region_movement.h"
#include "core/worldgen/region_generator.h"
#include "core/zone/zone_key.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::Ruleset;
using aetheria::serialize::encode_zone;
using aetheria::serialize::normalized_state_hash;
using aetheria::tests::test_ruleset;
using aetheria::world::find_region_path;
using aetheria::world::MovementPoints;
using aetheria::world::RegionMoveCommand;
using aetheria::world::RegionPosition;
using aetheria::world::RegionTiles;
using aetheria::world::RegionTurnPipeline;
using aetheria::world::RegionXY;
using aetheria::world::StableId;
using aetheria::world::TurnClock;
using aetheria::world::TurnStage;
using aetheria::zone::InMemoryZoneStore;
using aetheria::zone::Zone;

static_assert(std::is_integral_v<decltype(MovementPoints::current)>);
static_assert(std::is_integral_v<decltype(MovementPoints::per_xun)>);
static_assert(!std::is_floating_point_v<decltype(MovementPoints::current)>);

[[nodiscard]] RegionTiles plain_tiles(std::uint32_t width, std::uint32_t height,
                                      const Ruleset& ruleset = test_ruleset()) {
    RegionTiles tiles{width, height};
    std::fill(tiles.base.begin(), tiles.base.end(), *ruleset.find_terrain("terrain.grassland"));
    std::fill(tiles.relief.begin(), tiles.relief.end(), *ruleset.find_relief("relief.plain"));
    std::fill(tiles.feature.begin(), tiles.feature.end(), *ruleset.find_feature("feature.none"));
    std::fill(tiles.edges.begin(), tiles.edges.end(), *ruleset.find_edge("edge.none"));
    return tiles;
}

[[nodiscard]] entt::entity placeholder(Zone& zone) {
    const auto meta = zone.reg.view<aetheria::zone::ZoneMeta>();
    EXPECT_EQ(meta.size(), 1U);
    return *meta.begin();
}

void add_unit(Zone& zone, StableId stable_id, RegionXY tile, RegionXY target) {
    const auto entity = zone.reg.create();
    zone.reg.emplace<StableId>(entity, stable_id);
    zone.reg.emplace<RegionPosition>(entity, 0, tile);
    zone.reg.emplace<MovementPoints>(entity, 0, 4);
    zone.reg.emplace<RegionMoveCommand>(entity, target, false);
}

[[nodiscard]] std::unique_ptr<Zone> movement_zone(bool reverse_units = false) {
    const auto key = aetheria::zone::child_key(aetheria::zone::kRootZone, 1, 0);
    auto zone = std::make_unique<Zone>(key);
    std::get<aetheria::zone::RegionPayload>(zone->payload).layers.emplace(0, plain_tiles(12, 1));
    zone->reg.emplace<TurnClock>(placeholder(*zone), aetheria::time::Tick{0});
    if (reverse_units) {
        add_unit(*zone, StableId{20}, RegionXY{11, 0}, RegionXY{0, 0});
        add_unit(*zone, StableId{10}, RegionXY{0, 0}, RegionXY{11, 0});
    } else {
        add_unit(*zone, StableId{10}, RegionXY{0, 0}, RegionXY{11, 0});
        add_unit(*zone, StableId{20}, RegionXY{11, 0}, RegionXY{0, 0});
    }
    return zone;
}

TEST(RegionMovement, RoadRiverAndBridgeCostsComeFromEdgeDefinitions) {
    auto tiles = plain_tiles(2, 1);
    const RegionXY from{0, 0};
    const RegionXY to{1, 0};
    const auto plain = aetheria::world::region_step_cost(tiles, from, to, test_ruleset(), 1);
    const auto winter = aetheria::world::region_step_cost(tiles, from, to, test_ruleset(), 4);
    tiles.set_edge(from, to, *test_ruleset().find_edge("edge.road"));
    const auto road = aetheria::world::region_step_cost(tiles, from, to, test_ruleset(), 1);
    tiles.set_edge(from, to, *test_ruleset().find_edge("edge.river"));
    const auto river = aetheria::world::region_step_cost(tiles, from, to, test_ruleset(), 1);
    tiles.set_edge(from, to, *test_ruleset().find_edge("edge.bridge"));
    const auto bridge = aetheria::world::region_step_cost(tiles, from, to, test_ruleset(), 1);

    std::cout << "movement_cost plain=" << plain << " winter=" << winter << " road=" << road
              << " river=" << river << " bridge=" << bridge << '\n';
    EXPECT_GT(winter, plain);
    EXPECT_LT(road, plain);
    EXPECT_GT(river, plain);
    EXPECT_LT(bridge, river);
}

TEST(RegionPathfinding, AStarMatchesDijkstraForAtLeastOneHundredRandomPairs) {
    auto tiles = plain_tiles(16, 12);
    const auto road = *test_ruleset().find_edge("edge.road");
    auto random = UINT64_C(123456789);
    for (std::int16_t y = 0; y < 12; ++y) {
        for (std::int16_t x = 0; x < 16; ++x) {
            const RegionXY here{x, y};
            if (x + 1 < 16) {
                random = aetheria::worldgen::splitmix64(random);
                if ((random & 3U) == 0) {
                    tiles.set_edge(here, RegionXY{static_cast<std::int16_t>(x + 1), y}, road);
                }
            }
            if (y + 1 < 12) {
                random = aetheria::worldgen::splitmix64(random);
                if ((random & 3U) == 0) {
                    tiles.set_edge(here, RegionXY{x, static_cast<std::int16_t>(y + 1)}, road);
                }
            }
        }
    }
    for (std::size_t sample = 0; sample < 128; ++sample) {
        random = aetheria::worldgen::splitmix64(random);
        const RegionXY start{static_cast<std::int16_t>(random % 16U),
                             static_cast<std::int16_t>((random >> 8U) % 12U)};
        random = aetheria::worldgen::splitmix64(random);
        const RegionXY goal{static_cast<std::int16_t>(random % 16U),
                            static_cast<std::int16_t>((random >> 8U) % 12U)};
        const auto astar = find_region_path(tiles, start, goal, test_ruleset(), 1, 1);
        const auto dijkstra = find_region_path(tiles, start, goal, test_ruleset(), 1, 0);
        ASSERT_EQ(astar.has_value(), dijkstra.has_value());
        ASSERT_TRUE(astar.has_value());
        EXPECT_EQ(astar->cost, dijkstra->cost) << "sample=" << sample;
    }
}

TEST(RegionPathfinding, DeliberatelyInadmissibleHeuristicProducesASuboptimalPath) {
    auto tiles = plain_tiles(5, 3);
    const auto road = *test_ruleset().find_edge("edge.road");
    tiles.set_edge({0, 1}, {0, 0}, road);
    for (std::int16_t x = 0; x < 4; ++x) {
        tiles.set_edge({x, 0}, {static_cast<std::int16_t>(x + 1), 0}, road);
    }
    tiles.set_edge({4, 0}, {4, 1}, road);
    const auto dijkstra = find_region_path(tiles, {0, 1}, {4, 1}, test_ruleset(), 1, 0);
    const auto astar = find_region_path(tiles, {0, 1}, {4, 1}, test_ruleset(), 1, 1);
    const auto weighted = find_region_path(tiles, {0, 1}, {4, 1}, test_ruleset(), 1, 100);

    ASSERT_TRUE(dijkstra && astar && weighted);
    EXPECT_EQ(astar->cost, dijkstra->cost);
    EXPECT_GT(weighted->cost, dijkstra->cost);
    std::cout << "heuristic_probe dijkstra=" << dijkstra->cost << " admissible=" << astar->cost
              << " weighted=" << weighted->cost << '\n';
}

TEST(RegionTurn, PersistentCommandMovesForFiveXunAndCallsAllStagesInOrder) {
    auto zone = movement_zone();
    InMemoryZoneStore store{test_ruleset()};
    RegionTurnPipeline pipeline{test_ruleset(), store};
    zone->reg.clear<RegionMoveCommand>();
    pipeline.issue_move(*zone, StableId{10}, RegionXY{11, 0});
    pipeline.issue_move(*zone, StableId{20}, RegionXY{0, 0});
    std::vector<TurnStage> stages;
    for (std::size_t xun = 0; xun < 5; ++xun) {
        pipeline.advance_xun(*zone, [&](TurnStage stage) { stages.push_back(stage); });
        const auto units = zone->reg.view<const StableId, const RegionPosition>();
        for (const auto entity : units) {
            const auto id = units.get<const StableId>(entity).uid;
            const auto x = units.get<const RegionPosition>(entity).tile.x;
            const auto elapsed = static_cast<std::int16_t>(xun + 1U);
            EXPECT_EQ(x, id == 10 ? elapsed : static_cast<std::int16_t>(11 - elapsed));
        }
    }

    const auto units = zone->reg.view<const StableId, const RegionPosition>();
    for (const auto entity : units) {
        const auto id = units.get<const StableId>(entity).uid;
        const auto x = units.get<const RegionPosition>(entity).tile.x;
        EXPECT_EQ(x, id == 10 ? 5 : 6);
        EXPECT_TRUE(zone->reg.all_of<RegionMoveCommand>(entity));
    }
    ASSERT_EQ(stages.size(), 35U);
    for (std::size_t index = 0; index < stages.size(); ++index) {
        EXPECT_EQ(stages[index], static_cast<TurnStage>(index % 7U + 1U));
    }
    EXPECT_EQ(zone->reg.get<TurnClock>(placeholder(*zone)).now,
              aetheria::time::Tick{5 * static_cast<std::int64_t>(aetheria::time::kXun)});
    EXPECT_TRUE(store.contains(zone->key));
}

TEST(NormalizedStateHash, IgnoresEntityIdsLodAndPinnedButIncludesCommands) {
    auto first = movement_zone(false);
    auto second = movement_zone(true);
    second->lod = aetheria::zone::LodLevel::Absent;
    second->pinned = true;
    const auto first_hash = normalized_state_hash(*first, test_ruleset());
    const auto second_hash = normalized_state_hash(*second, test_ruleset());

    EXPECT_EQ(first_hash, second_hash);
    EXPECT_NE(encode_zone(*first, test_ruleset()), encode_zone(*second, test_ruleset()));
    const auto entity = *second->reg.view<RegionMoveCommand>().begin();
    ++second->reg.get<RegionMoveCommand>(entity).target.x;
    EXPECT_NE(first_hash, normalized_state_hash(*second, test_ruleset()));
}

TEST(RegionTurn, SaveLoadThenTenXunMatchesDirectTenXunByNormalizedHash) {
    auto direct = movement_zone(true);
    auto saved = movement_zone(false);
    InMemoryZoneStore direct_store{test_ruleset()};
    InMemoryZoneStore saved_store{test_ruleset()};
    saved_store.save(*saved);
    auto loaded = saved_store.load(saved->key);
    ASSERT_NE(loaded, nullptr);
    ASSERT_EQ(loaded->reg.view<const RegionMoveCommand>().size(), 2U);
    RegionTurnPipeline direct_pipeline{test_ruleset(), direct_store};
    RegionTurnPipeline loaded_pipeline{test_ruleset(), saved_store};
    for (std::size_t xun = 0; xun < 10; ++xun) {
        direct_pipeline.advance_xun(*direct);
        loaded_pipeline.advance_xun(*loaded);
    }
    const auto direct_hash = normalized_state_hash(*direct, test_ruleset());
    const auto loaded_hash = normalized_state_hash(*loaded, test_ruleset());
    std::cout << "ten_xun direct_hash=" << direct_hash << " loaded_hash=" << loaded_hash << '\n';
    EXPECT_EQ(direct_hash, loaded_hash);
}

TEST(RegionTurn, SameCommandsAndSameHistoryProduceIdenticalBytes) {
    auto first = movement_zone(false);
    auto second = movement_zone(false);
    InMemoryZoneStore first_store{test_ruleset()};
    InMemoryZoneStore second_store{test_ruleset()};
    RegionTurnPipeline first_pipeline{test_ruleset(), first_store};
    RegionTurnPipeline second_pipeline{test_ruleset(), second_store};
    for (std::size_t xun = 0; xun < 10; ++xun) {
        first_pipeline.advance_xun(*first);
        second_pipeline.advance_xun(*second);
    }
    EXPECT_EQ(encode_zone(*first, test_ruleset()), encode_zone(*second, test_ruleset()));
}

}  // namespace

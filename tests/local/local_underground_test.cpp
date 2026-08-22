#include "core/local/local_underground.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>

#include "tests/local/local_test_support.h"
#include "tests/support/performance.h"

namespace {

using aetheria::tests::kLocalCenter;
using aetheria::tests::kLocalSiteSeed;
using aetheria::tests::test_ruleset;
using aetheria::tests::underground_site_layer;

[[nodiscard]] aetheria::local::LocalSlowVars sample_slow(std::string_view structure) {
    const auto parent = underground_site_layer(structure);
    return aetheria::local::project_local_slow_vars(
        parent, kLocalCenter, kLocalSiteSeed,
        *test_ruleset().find_feature("feature.ancient_foundation"), test_ruleset());
}

[[nodiscard]] aetheria::local::UndergroundLocalSkeleton generate(std::string_view structure,
                                                                 std::uint64_t seed_offset = 0) {
    const auto seed =
        aetheria::local::derive_local_seed(kLocalSiteSeed, kLocalCenter.x, kLocalCenter.y) +
        seed_offset;
    return aetheria::local::build_underground_local_skeleton(sample_slow(structure), seed,
                                                             test_ruleset());
}

[[nodiscard]] std::size_t tile_index(aetheria::local::LocalXY tile) noexcept {
    return static_cast<std::size_t>(tile.y) * aetheria::local::kLocalWidth + tile.x;
}

void seal_doorway(aetheria::local::UndergroundLocalSkeleton& skeleton,
                  const aetheria::local::UndergroundCorridor& corridor) {
    const auto wall = test_ruleset().local_building_rules().wall_edge;
    const auto from = corridor.destination_outside;
    const auto to = corridor.destination_inside;
    auto& edges = skeleton.layers.at(corridor.z).edges;
    if (to.x == from.x + 1U && to.y == from.y) {
        edges[tile_index(from) * 4U + 1U] = wall;
        edges[tile_index(to) * 4U + 3U] = wall;
    } else if (from.x == to.x + 1U && to.y == from.y) {
        edges[tile_index(from) * 4U + 3U] = wall;
        edges[tile_index(to) * 4U + 1U] = wall;
    } else if (to.y == from.y + 1U && to.x == from.x) {
        edges[tile_index(from) * 4U + 2U] = wall;
        edges[tile_index(to) * 4U] = wall;
    } else if (from.y == to.y + 1U && to.x == from.x) {
        edges[tile_index(from) * 4U] = wall;
        edges[tile_index(to) * 4U + 2U] = wall;
    } else {
        FAIL() << "地下走廊目的門檻不是四鄰格";
    }
}

TEST(LocalUnderground, StructureControlsKindAndDepthWithoutDefaultingToTenLayers) {
    const auto mine = generate("building.mine_entrance");
    const auto dungeon = generate("building.dungeon_entrance");
    const auto ruin = generate("building.ruin_entrance");
    EXPECT_EQ(mine.kind, aetheria::rules::UndergroundKind::Mine);
    EXPECT_EQ(mine.depth, 2U);
    EXPECT_EQ(dungeon.kind, aetheria::rules::UndergroundKind::Dungeon);
    EXPECT_EQ(dungeon.depth, 3U);
    EXPECT_EQ(ruin.kind, aetheria::rules::UndergroundKind::Ruin);
    EXPECT_EQ(ruin.depth, 1U);
    EXPECT_EQ(mine.layers.size(), 3U);
    EXPECT_EQ(dungeon.layers.size(), 4U);
    EXPECT_EQ(ruin.layers.size(), 2U);
    EXPECT_TRUE(aetheria::local::all_underground_tiles_reachable(mine, test_ruleset()));
    EXPECT_TRUE(aetheria::local::all_underground_tiles_reachable(dungeon, test_ruleset()));
    EXPECT_TRUE(aetheria::local::all_underground_tiles_reachable(ruin, test_ruleset()));
    EXPECT_EQ(aetheria::local::count_unreachable_underground_rooms(dungeon, test_ruleset()), 0U);
    EXPECT_EQ(aetheria::local::count_unreachable_underground_rooms(ruin, test_ruleset()), 0U);
    std::cout << "local_route_c_depths mine=" << static_cast<unsigned>(mine.depth)
              << " dungeon=" << static_cast<unsigned>(dungeon.depth)
              << " ruin=" << static_cast<unsigned>(ruin.depth) << " unreachable_rooms=0\n";
}

TEST(LocalUnderground, SameSeedHasSameNormalizedHashAndVerticalLinksAreBidirectional) {
    const auto first = generate("building.dungeon_entrance");
    const auto repeated = generate("building.dungeon_entrance");
    const auto changed = generate("building.dungeon_entrance", 1);
    const auto hash = aetheria::local::hash_underground_local_skeleton(first);
    EXPECT_EQ(hash, aetheria::local::hash_underground_local_skeleton(repeated));
    EXPECT_NE(hash, aetheria::local::hash_underground_local_skeleton(changed));
    ASSERT_EQ(first.vertical_links.size(), first.depth);
    for (const auto& link : first.vertical_links) {
        const auto index = tile_index(link.tile);
        EXPECT_EQ(link.lower_z, link.upper_z - 1);
        EXPECT_EQ(first.layers.at(link.upper_z).overlay[index], aetheria::local::OverlayId::Stairs);
        EXPECT_EQ(first.layers.at(link.lower_z).overlay[index], aetheria::local::OverlayId::Stairs);
    }
    std::cout << "local_route_c_hash=" << hash << " vertical_links=" << first.vertical_links.size()
              << " bidirectional=1\n";
}

TEST(LocalUnderground, RuinRemovesSixtyToEightyPercentOfRouteAStructure) {
    const auto ruin = generate("building.ruin_entrance");
    ASSERT_GT(ruin.ruin_original_segments, 0U);
    const auto permille = static_cast<std::uint64_t>(ruin.ruin_removed_segments) * 1000U /
                          ruin.ruin_original_segments;
    EXPECT_GE(permille, 600U);
    EXPECT_LE(permille, 800U);
    EXPECT_TRUE(aetheria::local::valid_underground_invariants(ruin, test_ruleset()));
    std::cout << "local_route_c_ruin original_segments=" << ruin.ruin_original_segments
              << " removed_segments=" << ruin.ruin_removed_segments << " removed_percent="
              << static_cast<double>(ruin.ruin_removed_segments) * 100.0 /
                     ruin.ruin_original_segments
              << '\n';
}

TEST(LocalUnderground, SealedCorridorNegativeControlFindsUnreachableRoom) {
    auto dungeon = generate("building.dungeon_entrance");
    ASSERT_GT(dungeon.rooms.size(), 1U);
    ASSERT_FALSE(dungeon.corridors.empty());
    const auto corridor = dungeon.corridors.back();
    seal_doorway(dungeon, corridor);
    const auto unreachable =
        aetheria::local::count_unreachable_underground_rooms(dungeon, test_ruleset());
    EXPECT_EQ(unreachable, 1U);
    EXPECT_FALSE(aetheria::local::all_underground_tiles_reachable(dungeon, test_ruleset()));
    EXPECT_FALSE(aetheria::local::valid_underground_invariants(dungeon, test_ruleset()));
    std::cout << "local_route_c_negative rooms=" << dungeon.rooms.size()
              << " sealed_corridors=1 unreachable_rooms=" << unreachable
              << " invariant_rejected=1\n";
}

TEST(LocalUnderground, DungeonIsUnderTenMillisecondBudgetWithRealOutput) {
    const auto slow = sample_slow("building.dungeon_entrance");
    const auto seed =
        aetheria::local::derive_local_seed(kLocalSiteSeed, kLocalCenter.x, kLocalCenter.y);
    const auto witness =
        aetheria::local::build_underground_local_skeleton(slow, seed, test_ruleset());
    const auto minimum_milliseconds = aetheria::tests::minimum_milliseconds_after_warmup([&] {
        const auto start = std::chrono::steady_clock::now();
        const auto generated =
            aetheria::local::build_underground_local_skeleton(slow, seed, test_ruleset());
        EXPECT_GT(generated.excavated_count, 0U);
        EXPECT_GT(generated.rooms.size(), 1U);
        EXPECT_GT(generated.corridors.size(), 0U);
        return std::chrono::duration<double, std::milli>{std::chrono::steady_clock::now() - start}
            .count();
    });
    std::cout << "local_route_c_min_of_5_ms=" << minimum_milliseconds
              << " excavated_tiles=" << witness.excavated_count << " rooms=" << witness.rooms.size()
              << " corridors=" << witness.corridors.size() << " layers=" << witness.layers.size()
              << '\n';
    EXPECT_LT(minimum_milliseconds, 10.0);
}

}  // namespace

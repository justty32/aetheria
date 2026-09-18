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

TEST(DungeonPersistence, V20ColdRoundTripKeepsTriggeredAndClaimedStateInNormalizedHash) {
    aetheria::zone::Zone source{aetheria::tests::kLocal};
    auto& payload = std::get<aetheria::zone::LocalPayload>(source.payload);
    payload.dungeon.triggered_trap_uids = {11, 22};
    payload.dungeon.claimed_treasure_uids = {33, 44};
    const auto before = aetheria::serialize::normalized_state_hash(source, test_ruleset());
    const auto loaded = aetheria::serialize::decode_zone(
        aetheria::serialize::encode_zone(source, test_ruleset()), test_ruleset());
    const auto after = aetheria::serialize::normalized_state_hash(*loaded, test_ruleset());
    EXPECT_EQ(before, after);
    const auto& restored = std::get<aetheria::zone::LocalPayload>(loaded->payload).dungeon;
    EXPECT_EQ(restored, payload.dungeon);
    std::cout << "dungeon_v23_roundtrip triggered=2 claimed=2 hash=" << before << '\n';
}

TEST(DungeonPersistence, ClearedUsesExistingSiteFlagAndTenReloadOutputsConverge) {
    DungeonPersistentState state;
    std::array<std::uint64_t, 10> outputs{};
    for (std::size_t reload = 0; reload < outputs.size(); ++reload) {
        const auto dungeon = generate(UINT64_C(0xC1EA2ED), DungeonArchetype::Hybrid, 6, true, state);
        outputs[reload] = aetheria::local::available_treasure_value(dungeon);
        aetheria::local::claim_all_treasure(dungeon, state);
    }
    const auto uncleared = aetheria::local::available_treasure_value(
        generate(UINT64_C(0xC1EA2ED), DungeonArchetype::Hybrid, 6, false));
    EXPECT_GT(outputs[0], 0U);
    EXPECT_GT(uncleared, outputs[0] * 4U);
    EXPECT_TRUE(std::ranges::all_of(outputs | std::views::drop(1),
                                    [](std::uint64_t value) { return value == 0; }));
    std::cout << "dungeon_cleared_outputs=";
    for (std::size_t index = 0; index < outputs.size(); ++index) {
        std::cout << (index == 0 ? "" : ",") << outputs[index];
    }
    std::cout << " uncleared=" << uncleared << '\n';
}

TEST(DungeonHistory, UniqueTreasureOriginTracesToGeneratedHistoryTruth) {
    constexpr aetheria::worldgen::RegionSlowVariables slow{23, 128, 96};
    const auto region = aetheria::worldgen::build_skeleton(slow, UINT64_C(0xA37E21),
                                                           test_ruleset());
    ASSERT_FALSE(region.history.ancient_sites.cities.empty());
    const auto canonical = region.history.ancient_sites.cities.front().canonical_id;
    const auto origin = aetheria::local::dungeon_history_origin(
        slow.region_id, canonical, region.history);
    ASSERT_TRUE(origin.has_value());
    ASSERT_TRUE(aetheria::local::origin_matches_history(*origin, region.history));
    const auto dungeon = aetheria::local::generate_dungeon(
        UINT64_C(0xA37E21), DungeonArchetype::Hybrid, persistent(), {}, test_ruleset(), origin);
    const auto& deepest = dungeon.floors.back();
    const auto found = std::ranges::find_if(deepest.treasures,
                                            [](const auto& treasure) { return treasure.unique; });
    ASSERT_NE(found, deepest.treasures.end());
    ASSERT_TRUE(found->origin.has_value());
    EXPECT_EQ(*found->origin, *origin);
    EXPECT_TRUE(aetheria::local::origin_matches_history(*found->origin, region.history));
    std::cout << "dungeon_history_origin event_id=" << origin->event_id
              << " region=" << origin->region_id
              << " ancient_site=" << origin->ancient_site_canonical_id
              << " tier=" << static_cast<unsigned>(origin->ancient_site_tier)
              << " survived=" << origin->survived_cataclysm << '\n';
}

}  // namespace

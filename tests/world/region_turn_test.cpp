#include "core/serialize/normalized_state_hash.h"
#include "core/serialize/zone_codec.h"
#include "core/world/region_movement.h"
#include "tests/world/region_test_support.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::serialize::encode_zone;
using aetheria::serialize::normalized_state_hash;
using aetheria::tests::movement_zone;
using aetheria::tests::placeholder;
using aetheria::tests::test_ruleset;
using aetheria::world::MovementPoints;
using aetheria::world::RegionMoveCommand;
using aetheria::world::RegionPosition;
using aetheria::world::RegionTurnPipeline;
using aetheria::world::RegionXY;
using aetheria::world::StableId;
using aetheria::world::TurnClock;
using aetheria::world::TurnStage;
using aetheria::zone::InMemoryZoneStore;

static_assert(std::is_integral_v<decltype(MovementPoints::current)>);
static_assert(std::is_integral_v<decltype(MovementPoints::per_xun)>);
static_assert(!std::is_floating_point_v<decltype(MovementPoints::current)>);

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

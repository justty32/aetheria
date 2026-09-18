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

TEST(DungeonTrap, FourDataKindsUseExistingD100AndEnemyCanTriggerDamage) {
    ASSERT_EQ(test_ruleset().traps().size(), 4U);
    std::array<std::uint32_t, 4> kind_hits{};
    for (const auto& definition : test_ruleset().traps()) {
        ++kind_hits[static_cast<std::size_t>(definition.kind)];
    }
    EXPECT_EQ(kind_hits, (std::array<std::uint32_t, 4>{1, 1, 1, 1}));
    const auto pit = *test_ruleset().find_trap("trap.pit");
    const aetheria::local::DungeonTrap trap{701, pit, {8, 9}, 3, false};
    const aetheria::rules::Attributes attributes{20, 70, 30, 25};
    const auto detected = aetheria::local::detect_trap(trap, 12, attributes, test_ruleset());
    const auto undetected_without_light =
        aetheria::local::detect_trap(trap, 12, attributes, test_ruleset(), false);
    const auto disarmed = aetheria::local::disarm_trap(trap, 12, attributes, test_ruleset());
    ASSERT_TRUE(disarmed.has_value());
    EXPECT_TRUE(detected.success);
    EXPECT_FALSE(undetected_without_light.success);
    EXPECT_TRUE(disarmed->success);
    DungeonPersistentState state;
    const auto result = aetheria::local::trigger_trap(trap, 991, true, state, test_ruleset());
    EXPECT_TRUE(result.target_was_enemy);
    EXPECT_EQ(result.damage, 42);
    EXPECT_TRUE(result.newly_triggered);
    EXPECT_EQ(state.triggered_trap_uids, (std::vector<std::uint64_t>{701}));
    const auto second = aetheria::local::trigger_trap(trap, 991, true, state, test_ruleset());
    EXPECT_FALSE(second.newly_triggered);
    EXPECT_EQ(state.triggered_trap_uids.size(), 1U);
    std::cout << "dungeon_enemy_trap target=991 damage=" << result.damage
              << " persisted_triggered=" << state.triggered_trap_uids.size() << '\n';
}

TEST(DungeonBoss, ExistingTierGateMakesFindBreakthroughAndReturnLoopReal) {
    auto dungeon = generate();
    const auto without = aetheria::local::boss_damage(
        dungeon.boss, 120, aetheria::world::Significance::Ambient, test_ruleset());
    const auto* breakthrough = test_ruleset().breakthrough(dungeon.boss.required_breakthrough);
    ASSERT_NE(breakthrough, nullptr);
    const auto with = aetheria::local::boss_damage(
        dungeon.boss, 120, aetheria::world::Significance::Ambient, test_ruleset(), breakthrough);
    EXPECT_EQ(without, 0);
    EXPECT_EQ(with, 120);
    const auto event = aetheria::local::defeat_boss(dungeon.boss, dungeon.uid);
    EXPECT_FALSE(dungeon.boss.alive);
    EXPECT_GE(event.significance, aetheria::world::Significance::Region);
    const auto& deepest = dungeon.floors.back();
    EXPECT_TRUE(std::ranges::any_of(deepest.treasures, [](const auto& treasure) {
        return treasure.breakthrough.has_value();
    }));
    std::cout << "dungeon_boss_gate without_breakthrough=" << without
              << " with_breakthrough=" << with << " event_significance="
              << static_cast<unsigned>(event.significance) << '\n';
}

TEST(DungeonLight, ExhaustionDepthDistributionShowsFiniteSupplyPressure) {
    std::map<std::uint8_t, std::uint32_t> distribution;
    std::uint32_t never{};
    for (std::uint64_t index = 0; index < 100U; ++index) {
        const auto depth = aetheria::local::choose_dungeon_depth(
            aetheria::local::DungeonDepthSource::Ruin, index);
        const auto dungeon = generate(index * 31U + 7U, DungeonArchetype::Hybrid, depth);
        const auto exhausted = aetheria::local::light_exhaustion_depth(
            dungeon, static_cast<std::uint32_t>(24U + index % 25U));
        if (exhausted.has_value()) {
            ++distribution[*exhausted];
        } else {
            ++never;
        }
    }
    EXPECT_GE(distribution.size(), 3U);
    EXPECT_EQ(never, 0U);
    const auto lit = aetheria::local::dungeon_light_effect(true, test_ruleset());
    const auto unlit = aetheria::local::dungeon_light_effect(false, test_ruleset());
    EXPECT_LT(unlit.vision, lit.vision);
    EXPECT_LT(unlit.hit_modifier, lit.hit_modifier);
    EXPECT_LT(unlit.detection_modifier, lit.detection_modifier);
    std::cout << "dungeon_light_exhaustion n=100";
    for (const auto [depth, count] : distribution) {
        std::cout << " depth" << static_cast<unsigned>(depth) << '=' << count;
    }
    std::cout << " never=" << never << " vision=" << static_cast<unsigned>(lit.vision) << "->"
              << static_cast<unsigned>(unlit.vision) << " hit_modifier=" << unlit.hit_modifier
              << " detection_modifier=" << unlit.detection_modifier << '\n';
}

}  // namespace

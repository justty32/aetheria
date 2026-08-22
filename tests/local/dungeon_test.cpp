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

namespace {

using aetheria::local::DungeonGenerated;
using aetheria::local::DungeonPersistentState;
using aetheria::rules::DungeonArchetype;
using aetheria::tests::test_ruleset;

[[nodiscard]] aetheria::site::PersistentDungeon persistent(std::uint16_t depth = 6,
                                                            bool cleared = false) {
    return {UINT64_C(0xD006E001), "place.test_dungeon", cleared, depth};
}

[[nodiscard]] DungeonGenerated generate(std::uint64_t seed = UINT64_C(0xD06E0A),
                                        DungeonArchetype archetype = DungeonArchetype::Hybrid,
                                        std::uint16_t depth = 6, bool cleared = false,
                                        const DungeonPersistentState& state = {}) {
    return aetheria::local::generate_dungeon(seed, archetype, persistent(depth, cleared), state,
                                             test_ruleset());
}

[[nodiscard]] double correlation(const std::vector<double>& left,
                                 const std::vector<double>& right) {
    const auto left_mean = std::accumulate(left.begin(), left.end(), 0.0) / left.size();
    const auto right_mean = std::accumulate(right.begin(), right.end(), 0.0) / right.size();
    double numerator{};
    double left_square{};
    double right_square{};
    for (std::size_t index = 0; index < left.size(); ++index) {
        const auto left_delta = left[index] - left_mean;
        const auto right_delta = right[index] - right_mean;
        numerator += left_delta * right_delta;
        left_square += left_delta * left_delta;
        right_square += right_delta * right_delta;
    }
    return numerator / std::sqrt(left_square * right_square);
}

class DungeonArchetypeTest : public testing::TestWithParam<DungeonArchetype> {};

TEST_P(DungeonArchetypeTest, SharedGeneratorMakesEveryDepthStrictlyHarder) {
    const auto dungeon = generate(UINT64_C(0xC0FFEE), GetParam(), 7);
    ASSERT_TRUE(aetheria::local::valid_dungeon(dungeon, test_ruleset()));
    ASSERT_EQ(dungeon.floors.size(), 7U);
    for (std::size_t index = 1; index < dungeon.floors.size(); ++index) {
        EXPECT_GT(dungeon.floors[index].difficulty, dungeon.floors[index - 1U].difficulty);
        EXPECT_GT(dungeon.floors[index].enemy_count, dungeon.floors[index - 1U].enemy_count);
        EXPECT_GT(dungeon.floors[index].light_cost, dungeon.floors[index - 1U].light_cost);
        EXPECT_GT(dungeon.floors[index].retreat_cost, dungeon.floors[index - 1U].retreat_cost);
    }
}

INSTANTIATE_TEST_SUITE_P(NaturalArtificialHybrid, DungeonArchetypeTest,
                         testing::Values(DungeonArchetype::Natural,
                                         DungeonArchetype::Artificial,
                                         DungeonArchetype::Hybrid));

TEST(DungeonGeneration, ArchetypesAreParametersOfOneLayoutPipeline) {
    const auto natural = generate(101, DungeonArchetype::Natural);
    const auto artificial = generate(101, DungeonArchetype::Artificial);
    const auto hybrid = generate(101, DungeonArchetype::Hybrid);
    const auto room_counts = std::array{natural.floors.front().rooms.size(),
                                        artificial.floors.front().rooms.size(),
                                        hybrid.floors.front().rooms.size()};
    EXPECT_EQ(room_counts, (std::array<std::size_t, 3>{7, 8, 8}));
    const auto count_natural = [](const auto& dungeon) {
        return std::ranges::count_if(dungeon.floors.front().rooms,
                                     [](const auto& room) { return room.natural; });
    };
    EXPECT_EQ(count_natural(natural), 7);
    EXPECT_EQ(count_natural(artificial), 0);
    EXPECT_GT(count_natural(hybrid), 0);
    EXPECT_LT(count_natural(hybrid), 8);
    std::cout << "dungeon_shared_pipeline rooms=" << room_counts[0] << ',' << room_counts[1]
              << ',' << room_counts[2] << " natural_rooms=" << count_natural(natural) << ','
              << count_natural(artificial) << ',' << count_natural(hybrid) << '\n';
}

TEST(DungeonGeneration, SiteSourceOwnsDepthBoundsAndStoryUsesExplicitData) {
    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        EXPECT_GE(aetheria::local::choose_dungeon_depth(
                      aetheria::local::DungeonDepthSource::BeastLair, seed),
                  1U);
        EXPECT_LE(aetheria::local::choose_dungeon_depth(
                      aetheria::local::DungeonDepthSource::BeastLair, seed),
                  2U);
        EXPECT_GE(aetheria::local::choose_dungeon_depth(
                      aetheria::local::DungeonDepthSource::Mine, seed),
                  3U);
        EXPECT_LE(aetheria::local::choose_dungeon_depth(
                      aetheria::local::DungeonDepthSource::Mine, seed),
                  6U);
        EXPECT_GE(aetheria::local::choose_dungeon_depth(
                      aetheria::local::DungeonDepthSource::Ruin, seed),
                  5U);
        EXPECT_LE(aetheria::local::choose_dungeon_depth(
                      aetheria::local::DungeonDepthSource::Ruin, seed),
                  10U);
    }
    EXPECT_EQ(aetheria::local::choose_dungeon_depth(
                  aetheria::local::DungeonDepthSource::Story, 0, 13),
              13U);
}

TEST(DungeonGeneration, EntranceClueCorrelatesWithActualDeepestDifficultyAcrossOneHundred) {
    std::vector<double> clues;
    std::vector<double> actual;
    for (std::uint64_t index = 0; index < 100U; ++index) {
        const auto depth = aetheria::local::choose_dungeon_depth(
            aetheria::local::DungeonDepthSource::Ruin, index * 17U + 3U);
        const auto dungeon = generate(index * UINT64_C(0x9E3779B97F4A7C15),
                                      static_cast<DungeonArchetype>(index % 3U), depth);
        clues.push_back(dungeon.entrance_clue.predicted_deepest_difficulty);
        actual.push_back(dungeon.floors.back().difficulty);
    }
    const auto measured = correlation(clues, actual);
    EXPECT_GT(measured, 0.95);
    std::cout << "dungeon_clue_correlation n=100 pearson=" << measured << '\n';
}

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
    std::cout << "dungeon_v20_roundtrip triggered=2 claimed=2 hash=" << before << '\n';
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

TEST(DungeonGeneration, SameSeedIsDeterministicAndMinimumOfFiveIsUnderBudget) {
    const auto first = generate(UINT64_C(0x515151), DungeonArchetype::Hybrid, 10);
    const auto repeated = generate(UINT64_C(0x515151), DungeonArchetype::Hybrid, 10);
    EXPECT_EQ(aetheria::local::hash_dungeon(first), aetheria::local::hash_dungeon(repeated));
    const auto minimum = aetheria::tests::minimum_milliseconds_after_warmup([&] {
        const auto start = std::chrono::steady_clock::now();
        const auto generated = generate(UINT64_C(0x515151), DungeonArchetype::Hybrid, 10);
        EXPECT_EQ(generated.floors.size(), 10U);
        return std::chrono::duration<double, std::milli>{std::chrono::steady_clock::now() - start}
            .count();
    });
    EXPECT_LT(minimum, 10.0);
    std::cout << "dungeon_deterministic_hash=" << aetheria::local::hash_dungeon(first)
              << " min_of_5_ms=" << minimum << " floors=" << first.floors.size() << '\n';
}

}  // namespace

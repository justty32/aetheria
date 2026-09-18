#include "tests/site/site_combat_test_support.h"

#include "core/world/combat_scaling.h"
#include "tests/support/performance.h"
#include "tests/support/ruleset_fixture.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::site::CohortFacing;
using aetheria::site::CohortFormation;
using aetheria::site::CohortRole;
using aetheria::site::SiteCohort;

using aetheria::tests::site_combat::neutral;

[[nodiscard]] aetheria::rules::CombatInput matchup(std::int32_t power_a = 100'000,
                                                    std::int32_t power_b = 100'000) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    return {{power_a, neutral(), {}, 0},
            {power_b, neutral(), {}, 0},
            rules.default_exponent,
            1};
}

TEST(SiteCombat, SimulationIsDeterministicAndUsesRealCohortConditions) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    const auto input = matchup(95'000, 100'000);
    const auto region = aetheria::rules::resolve_region_combat(input, rules);
    const aetheria::site::SiteBattleInput site_input{
        .side_a = input.side_a,
        .side_b = input.side_b,
        .region_expected_loss_a = region.loss_a,
        .region_expected_loss_b = region.loss_b,
        .modifier_scale = rules.modifier_scale,
        .event_id = 77,
        .sample_seed = 123,
    };
    const auto first = aetheria::site::simulate_site_battle(site_input);
    const auto second = aetheria::site::simulate_site_battle(site_input);
    EXPECT_EQ(first, second);
    EXPECT_EQ(first.telemetry.turns, 16);
    EXPECT_EQ(first.telemetry.maximum_movement_tiles, 3);
    EXPECT_GT(first.telemetry.pressure_a_to_b, 0);
    EXPECT_GT(first.telemetry.pressure_b_to_a, 0);
    const auto met = first.telemetry.condition_met;
    const auto unmet = first.telemetry.condition_not_met;
    EXPECT_GT(met[0] + met[1] + met[2] + met[3], 0U);
    EXPECT_GT(unmet[0] + unmet[1] + unmet[2] + unmet[3], 0U);
    std::cout << "site_sim_deterministic loss_a_b=" << first.loss_a << '/'
              << first.loss_b << " pressure_a_b=" << first.telemetry.pressure_a_to_b
              << '/' << first.telemetry.pressure_b_to_a << " condition_met="
              << met[0] << '/' << met[1] << '/' << met[2] << '/' << met[3]
              << " condition_unmet=" << unmet[0] << '/' << unmet[1] << '/'
              << unmet[2] << '/' << unmet[3] << '\n';
}

TEST(SiteCombat, ResultReducesOnceThroughExistingCombatEventChannel) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    aetheria::world::CombatExecutionCounters counters;
    const auto result = aetheria::world::resolve_scaled_combat(
        matchup(), rules, aetheria::world::CombatLayer::Site, 91, 7, {}, {}, &counters);
    aetheria::world::CombatEventState state{
        .event_id = 91,
        .initial_power_a = 100'000,
        .initial_power_b = 100'000,
        .named = {},
    };
    EXPECT_TRUE(aetheria::world::demote_combat_event(state, result, &counters));
    EXPECT_FALSE(aetheria::world::demote_combat_event(state, result, &counters));
    EXPECT_EQ(state.accumulated_loss_a, result.loss_a);
    EXPECT_EQ(state.accumulated_loss_b, result.loss_b);
    EXPECT_EQ(counters.site_reduction_writes, 1U);
    EXPECT_EQ(counters.region_face_damage_writes, 0U);
    std::cout << "site_combat_reduction writes=" << counters.site_reduction_writes
              << " duplicate_writes=0 region_face_damage_writes="
              << counters.region_face_damage_writes << " reduced_loss_a_b="
              << state.accumulated_loss_a << '/' << state.accumulated_loss_b << '\n';
}

TEST(SiteCombat, OneBattleFitsBudgetUsingMinimumOfFive) {
    using Clock = std::chrono::steady_clock;
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    const auto input = matchup();
    const auto region = aetheria::rules::resolve_region_combat(input, rules);
    const aetheria::site::SiteBattleInput site_input{
        .side_a = input.side_a,
        .side_b = input.side_b,
        .region_expected_loss_a = region.loss_a,
        .region_expected_loss_b = region.loss_b,
        .modifier_scale = rules.modifier_scale,
        .event_id = 99,
        .sample_seed = 42,
    };
    const auto milliseconds = aetheria::tests::minimum_milliseconds_after_warmup([&] {
        const auto start = Clock::now();
        const auto result = aetheria::site::simulate_site_battle(site_input);
        EXPECT_GT(result.loss_a + result.loss_b, 0);
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    });
    EXPECT_LT(milliseconds, 5.0);
    std::cout << "site_battle_performance warmup=1 measured=5 minimum_ms="
              << milliseconds << " budget_ms=5\n";
}

}  // namespace

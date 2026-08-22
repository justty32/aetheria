#include "core/site/site_combat.h"

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

[[nodiscard]] aetheria::rules::CombatModifiers neutral() {
    const auto scale = aetheria::tests::test_ruleset().combat_rules().modifier_scale;
    return {scale, scale, scale, scale, scale};
}

[[nodiscard]] SiteCohort cohort(CohortRole role, std::int16_t x,
                                std::int16_t y) {
    return {.cohort_id = 1,
            .role = role,
            .formation = role == CohortRole::Spear ? CohortFormation::SpearWall
                                                   : CohortFormation::Line,
            .facing = CohortFacing::East,
            .position = {x, y},
            .power = 100'000,
            .formation_integrity_permyriad = 10'000,
            .morale_permyriad = 10'000};
}

[[nodiscard]] aetheria::rules::CombatInput matchup(std::int32_t power_a = 100'000,
                                                    std::int32_t power_b = 100'000) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    return {{power_a, neutral(), {}, 0},
            {power_b, neutral(), {}, 0},
            rules.default_exponent,
            1};
}

TEST(SiteCombat, FifteenMinuteStrideMovesThreeTilesAndPreservesManeuver) {
    EXPECT_EQ(aetheria::site::kSiteEngagementStrideSeconds, 900);
    EXPECT_EQ(aetheria::site::site_engagement_movement_tiles(), 3);
    const auto turns_to_cross =
        (63 + aetheria::site::site_engagement_movement_tiles() - 1) /
        aetheria::site::site_engagement_movement_tiles();
    EXPECT_EQ(turns_to_cross, 21);
    EXPECT_GT(turns_to_cross, aetheria::site::kSiteCombatTurns);
    std::cout << "site_stride seconds=900 metres_per_tile=125 speed_mph=1500"
              << " movement_tiles=3 turns_to_cross_63=21 battle_turns=16\n";
}

TEST(SiteCombat, EveryMatchupBonusRequiresItsTacticalCondition) {
    const auto modifiers = neutral();
    std::array<std::int32_t, 4> met{};
    std::array<std::int32_t, 4> unmet{};

    auto spear = cohort(CohortRole::Spear, 10, 10);
    const auto cavalry = cohort(CohortRole::Cavalry, 11, 10);
    met[0] = aetheria::site::resolve_site_attack(spear, cavalry, modifiers, modifiers)
                 .final_loss;
    spear.formation_integrity_permyriad = 7'999;
    unmet[0] = aetheria::site::resolve_site_attack(spear, cavalry, modifiers, modifiers)
                   .final_loss;

    auto flanking_cavalry = cohort(CohortRole::Cavalry, 10, 9);
    auto archer = cohort(CohortRole::Archer, 10, 10);
    met[1] = aetheria::site::resolve_site_attack(flanking_cavalry, archer, modifiers,
                                                modifiers)
                 .final_loss;
    flanking_cavalry.position = {11, 10};
    unmet[1] = aetheria::site::resolve_site_attack(flanking_cavalry, archer, modifiers,
                                                  modifiers)
                   .final_loss;

    archer = cohort(CohortRole::Archer, 10, 10);
    spear = cohort(CohortRole::Spear, 14, 10);
    met[2] = aetheria::site::resolve_site_attack(archer, spear, modifiers, modifiers)
                 .final_loss;
    spear.position = {13, 10};
    unmet[2] = aetheria::site::resolve_site_attack(archer, spear, modifiers, modifiers)
                   .final_loss;

    auto siege = cohort(CohortRole::Siege, 10, 10);
    auto wall = cohort(CohortRole::Spear, 19, 10);
    siege.in_cover = true;
    wall.behind_wall = true;
    met[3] = aetheria::site::resolve_site_attack(siege, wall, modifiers, modifiers)
                 .final_loss;
    siege.moved_tiles_last_turn = 1;
    unmet[3] = aetheria::site::resolve_site_attack(siege, wall, modifiers, modifiers)
                   .final_loss;

    EXPECT_EQ(met, (std::array<std::int32_t, 4>{1'500, 1'600, 1'400, 1'800}));
    EXPECT_EQ(unmet, (std::array<std::int32_t, 4>{1'000, 1'000, 1'000, 1'000}));
    for (std::size_t index = 0; index < met.size(); ++index) {
        EXPECT_GT(met[index], unmet[index]);
    }
    std::cout << "site_matchups spear_vs_cavalry=" << met[0] << '/' << unmet[0]
              << " cavalry_vs_archer=" << met[1] << '/' << unmet[1]
              << " archer_vs_spear=" << met[2] << '/' << unmet[2]
              << " siege_vs_wall=" << met[3] << '/' << unmet[3]
              << " false_samples_each=1\n";
}

TEST(SiteCombat, ExistingTerrainAndCommandModifiersAffectCohortExchange) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    auto attacker = cohort(CohortRole::Spear, 10, 10);
    const auto defender = cohort(CohortRole::Archer, 11, 10);
    auto attacker_modifiers = neutral();
    auto defender_modifiers = neutral();
    const auto baseline = aetheria::site::resolve_site_attack(
        attacker, defender, attacker_modifiers, defender_modifiers);
    attacker_modifiers.command = 1'200;
    const auto commanded = aetheria::site::resolve_site_attack(
        attacker, defender, attacker_modifiers, defender_modifiers,
        rules.modifier_scale);
    defender_modifiers.terrain = 1'500;
    const auto fortified = aetheria::site::resolve_site_attack(
        attacker, defender, attacker_modifiers, defender_modifiers,
        rules.modifier_scale);
    EXPECT_EQ(rules.modifier_scale, 1'000);
    EXPECT_EQ(baseline.final_loss, 1'000);
    EXPECT_EQ(commanded.final_loss, 1'200);
    EXPECT_EQ(fortified.final_loss, 800);
    std::cout << "site_modifiers baseline=1000 command_1200=1200"
              << " command_1200_vs_terrain_1500=800\n";
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

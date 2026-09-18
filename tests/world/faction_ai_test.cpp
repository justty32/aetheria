#include "core/rules/combat.h"
#include "core/world/faction_ai.h"
#include "tests/support/performance.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "tests/world/faction_ai_test_support.h"

namespace {

using namespace aetheria;
using tests::test_ruleset;
using world::FactionId;
using world::WorldDiplomacyState;

using namespace aetheria::tests::faction_ai;



TEST(FactionAiRules, LoadsOneSevenWeightPersonalityPerFaction) {
    const auto& rules = test_ruleset().civilization_rules().faction_ai;
    ASSERT_EQ(rules.definitions.size(), 3U);
    EXPECT_EQ(rules.definitions[0].id, "faction.iron_crown");
    EXPECT_EQ(rules.definitions[0].aggression, 90);
    EXPECT_EQ(rules.definitions[1].commerce, 95);
    EXPECT_EQ(rules.definitions[2].piety, 95);
    EXPECT_EQ(rules.goal_switch_threshold, 80);
}

TEST(FactionAiGoal, HysteresisReducesSwitchesAcrossOneHundredXun) {
    WorldDiplomacyState world{3, 0x1234, test_ruleset()};
    world.set_faction_truth(FactionId{1}, 1000, 1000);
    const auto view = world::make_faction_view(world, FactionId{1});
    const ai::FactionPersonality neutral{};
    ai::FactionMindState inertial;
    ai::FactionMindState zero_threshold;
    for (std::int64_t xun = 0; xun < 100; ++xun) {
        static_cast<void>(ai::update_faction_goal(
            view, neutral, {0x55aa, xun, 80}, inertial));
        static_cast<void>(ai::update_faction_goal(
            view, neutral, {0x55aa, xun, 0}, zero_threshold));
    }
    EXPECT_LT(inertial.goal_switches, zero_threshold.goal_switches);
    EXPECT_GE(zero_threshold.goal_switches,
              inertial.goal_switches + 20U);
    std::cout << "faction_ai goal_switches threshold80=" << inertial.goal_switches
              << " threshold0=" << zero_threshold.goal_switches << '\n';
}

TEST(FactionAiDeterminism, EqualUtilityUsesFactionIdNotIterationOrder) {
    const std::array forward{
        ai::ScoredFactionAction{{ai::FactionActionKind::DeclareWar, 1, 3}, 500},
        ai::ScoredFactionAction{{ai::FactionActionKind::DeclareWar, 1, 2}, 500},
    };
    const std::array reversed{forward[1], forward[0]};
    EXPECT_EQ(ai::select_highest_utility(forward).command.target, 2);
    EXPECT_EQ(ai::select_highest_utility(reversed).command.target, 2);
}

TEST(FactionAiPersonality, SameViewAndSeedChooseDifferentActions) {
    auto first = observed_world(44);
    auto second = observed_world(44);
    auto& war_mind = first.faction_mind(FactionId{1});
    auto& trade_mind = second.faction_mind(FactionId{1});
    war_mind.forced_goal = ai::FactionGoal::Conquer;
    trade_mind.forced_goal = ai::FactionGoal::Prosper;
    const auto war = world::plan_faction_ai_xun(
        first, FactionId{1}, {3, 0, true, true, false}, time::Tick{0},
        test_ruleset());
    const auto trade = ai::decide_full(
        world::make_faction_view(second, FactionId{1}),
        {.expansion = 10, .aggression = 5, .fidelity = 80, .commerce = 100,
         .piety = 10, .caution = 60, .resentment = 5},
        {44, 0, 80}, trade_mind,
        {[](std::int32_t own, std::int32_t target, const void* rules) noexcept {
             return world::forecast_region_battle(
                 own, target, *static_cast<const rules::CombatRules*>(rules));
         },
         &test_ruleset().combat_rules()});
    EXPECT_EQ(war.decision.command.kind, ai::FactionActionKind::DeclareWar);
    EXPECT_EQ(trade.command.kind, ai::FactionActionKind::Develop);
    std::cout << "faction_ai personality_actions war="
              << static_cast<int>(war.decision.command.kind) << " trade="
              << static_cast<int>(trade.command.kind) << '\n';
}

TEST(FactionAiCombat, PlayerAndAiForecastCallTheSameRegionFormula) {
    const auto& rules = test_ruleset().combat_rules();
    const auto ai_forecast = world::forecast_region_battle(2000, 800, rules);
    const rules::CombatModifiers neutral{rules.modifier_scale, rules.modifier_scale,
                                         rules.modifier_scale, rules.modifier_scale,
                                         rules.modifier_scale};
    const auto player_result = rules::resolve_region_combat(
        {{2000, neutral, {}, 0}, {800, neutral, {}, 0},
         rules.default_exponent, 1},
        rules);
    EXPECT_EQ(ai_forecast, ai::BattleAssessment::LikelyWin);
    EXPECT_EQ(player_result.outcome, rules::Outcome::SideBRouted);
}

TEST(FactionAiCommand, PlayerAutopilotAndNpcUseSamePlanAndExecutionPath) {
    auto player = observed_world(8181);
    auto npc = observed_world(8181);
    world::set_managed_faction_goal(player, FactionId{1}, ai::FactionGoal::Prosper);
    world::set_managed_faction_goal(npc, FactionId{1}, ai::FactionGoal::Prosper);
    const auto player_report = world::advance_faction_ai_xun(
        player, FactionId{1}, {3, 0, true, true, false}, time::Tick{0}, test_ruleset());
    const auto npc_report = world::advance_faction_ai_xun(
        npc, FactionId{1}, {3, 0, true, true, false}, time::Tick{0}, test_ruleset());
    EXPECT_EQ(player_report.decision.command, npc_report.decision.command);
    EXPECT_EQ(player.faction_truth(FactionId{1}), npc.faction_truth(FactionId{1}));
}

TEST(FactionAiDiplomacy, DefensiveAllyJoinsOrLosesTrust) {
    auto joined = observed_world(9);
    auto refused = observed_world(9);
    const auto alliance = *test_ruleset().find_treaty("treaty.defensive_alliance");
    static_cast<void>(joined.start_treaty(alliance, FactionId{2}, FactionId{1}, time::Tick{0}));
    static_cast<void>(refused.start_treaty(alliance, FactionId{2}, FactionId{1}, time::Tick{0}));
    world::answer_defensive_alliance_call(joined, FactionId{2}, FactionId{1}, FactionId{3}, true,
                                          time::Tick{0}, test_ruleset());
    world::answer_defensive_alliance_call(refused, FactionId{2}, FactionId{1}, FactionId{3}, false,
                                          time::Tick{0}, test_ruleset());
    EXPECT_TRUE(std::ranges::any_of(joined.wars(), [](const auto& war) {
        return war.active && war.participants[0] == FactionId{2} &&
               war.participants[1] == FactionId{3};
    }));
    EXPECT_EQ(refused.relation(FactionId{1}, FactionId{2}).trust, -5000);
}

}  // namespace

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

namespace {

using namespace aetheria;
using tests::test_ruleset;
using world::FactionId;
using world::WorldDiplomacyState;

[[nodiscard]] WorldDiplomacyState observed_world(std::uint64_t seed = 7) {
    WorldDiplomacyState world{3, seed, test_ruleset()};
    world.set_faction_truth(FactionId{1}, 1200, 900);
    world.set_faction_truth(FactionId{2}, 1200, 900);
    world.set_faction_truth(FactionId{3}, 1200, 900);
    for (std::uint16_t observer = 1; observer <= 3; ++observer) {
        for (std::uint16_t target = 1; target <= 3; ++target) {
            if (observer != target) {
                world.observe_faction(FactionId{observer}, FactionId{target}, 0,
                                      time::Tick{0}, 3);
            }
        }
    }
    return world;
}

[[nodiscard]] std::int32_t utility_for(const ai::FactionDecision& decision,
                                       ai::FactionActionKind kind,
                                       ai::FactionKey target) {
    const auto found = std::ranges::find_if(decision.scored_actions, [&](const auto& action) {
        return action.command.kind == kind && action.command.target == target;
    });
    if (found == decision.scored_actions.end()) {
        throw std::runtime_error{"測試找不到預期效用項"};
    }
    return found->utility;
}

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

TEST(FactionAiLod, ThreeLevelsRunWithSeparatedCostsAndUnbiasedPowerCurves) {
    std::int64_t full_total{};
    std::int64_t simplified_total{};
    std::int64_t statistical_total{};
    std::size_t full_count{};
    std::size_t simplified_count{};
    std::size_t statistical_count{};
    std::size_t positive_simplified{};
    std::size_t negative_simplified{};
    std::size_t positive_statistical{};
    std::size_t negative_statistical{};
    std::size_t full_cost{};
    std::size_t simplified_cost{};
    std::size_t statistical_cost{};
    constexpr std::size_t samples = 24;
    for (std::size_t sample = 0; sample < samples; ++sample) {
        auto world = observed_world(1000 + sample);
        world.faction_mind(FactionId{1}).forced_goal = ai::FactionGoal::Prosper;
        for (std::int64_t xun = 0; xun < 1000; ++xun) {
            const auto now = time::Tick{xun * static_cast<std::int64_t>(time::kXun)};
            const auto full = world::advance_faction_ai_xun(
                world, FactionId{1}, {3, 0, true, true, false}, now, test_ruleset());
            const auto simplified = world::advance_faction_ai_xun(
                world, FactionId{2}, {0, 10, true, false, false}, now, test_ruleset());
            const auto statistical = world::advance_faction_ai_xun(
                world, FactionId{3}, {0, 10, false, false, false}, now, test_ruleset());
            ++full_count;
            ++simplified_count;
            ++statistical_count;
            full_cost += full.utility_evaluations;
            simplified_cost += simplified.utility_evaluations;
            statistical_cost += statistical.utility_evaluations;
        }
        const auto full_power = *world.faction_truth(FactionId{1});
        const auto simplified_power = *world.faction_truth(FactionId{2});
        const auto statistical_power = *world.faction_truth(FactionId{3});
        const auto full_value = static_cast<std::int64_t>(full_power.military_power) +
                                full_power.economic_power;
        const auto simplified_value =
            static_cast<std::int64_t>(simplified_power.military_power) +
            simplified_power.economic_power;
        const auto statistical_value =
            static_cast<std::int64_t>(statistical_power.military_power) +
            statistical_power.economic_power;
        full_total += full_value;
        simplified_total += simplified_value;
        statistical_total += statistical_value;
        positive_simplified += simplified_value > full_value ? 1U : 0U;
        negative_simplified += simplified_value < full_value ? 1U : 0U;
        positive_statistical += statistical_value > full_value ? 1U : 0U;
        negative_statistical += statistical_value < full_value ? 1U : 0U;
    }
    const auto simplified_error =
        (simplified_total - full_total) * 10000 / full_total;
    const auto statistical_error =
        (statistical_total - full_total) * 10000 / full_total;
    EXPECT_LT(std::abs(simplified_error), 500);
    EXPECT_LT(std::abs(statistical_error), 500);
    EXPECT_GT(positive_simplified, 0U);
    EXPECT_GT(negative_simplified, 0U);
    EXPECT_GT(positive_statistical, 0U);
    EXPECT_GT(negative_statistical, 0U);
    EXPECT_GT(full_cost / full_count, simplified_cost / simplified_count);
    EXPECT_GT(simplified_cost / simplified_count, statistical_cost / statistical_count);
    std::cout << "faction_ai lod counts=" << full_count << ',' << simplified_count << ','
              << statistical_count << " cost_per_xun=" << full_cost / full_count << ','
              << simplified_cost / simplified_count << ','
              << statistical_cost / statistical_count
              << " relative_error_permyriad simplified=" << simplified_error
              << " statistical=" << statistical_error << " signs simplified=+"
              << positive_simplified << "/-" << negative_simplified << " statistical=+"
              << positive_statistical << "/-" << negative_statistical << '\n';
}

TEST(FactionAiLod, AllFactionEntryUsesFieldAndRunsEveryFactionOnce) {
    auto world = observed_world(31337);
    const std::array attention{
        world::FactionAttention{},
        world::FactionAttention{0, 100, true, true, false},
        world::FactionAttention{0, 100, true, false, false},
        world::FactionAttention{0, 100, false, false, false},
    };
    const auto reports = world::advance_all_factions_ai_xun(
        world, attention, time::Tick{0}, test_ruleset());
    ASSERT_EQ(reports.size(), 3U);
    EXPECT_EQ(reports[0].lod, ai::FactionAiLod::Full);
    EXPECT_EQ(reports[1].lod, ai::FactionAiLod::Simplified);
    EXPECT_EQ(reports[2].lod, ai::FactionAiLod::Statistical);
    EXPECT_EQ(reports[0].decision.command.issuer, 1);
    EXPECT_EQ(reports[1].decision.command.issuer, 2);
    EXPECT_EQ(reports[2].decision.command.issuer, 3);
}

TEST(FactionAiKnowledge, UnderestimationCausesWarsThatLoseAgainstTruth) {
    WorldDiplomacyState world{3, 0x9988, test_ruleset()};
    world.set_faction_truth(FactionId{1}, 1000, 700);
    world.set_faction_truth(FactionId{2}, 1600, 600);
    world.set_faction_truth(FactionId{3}, 800, 800);
    world.faction_mind(FactionId{1}).forced_goal = ai::FactionGoal::Conquer;
    std::size_t mistaken_losses{};
    for (std::int64_t xun = 0; xun < 1000; ++xun) {
        const auto now = time::Tick{xun * static_cast<std::int64_t>(time::kXun)};
        world.observe_faction(FactionId{1}, FactionId{2}, 8000, now, 12);
        const auto report = world::plan_faction_ai_xun(
            world, FactionId{1}, {4, 0, true, false, true}, now, test_ruleset());
        const auto estimate = world::make_faction_view(world, FactionId{1}).estimate(2);
        if (report.decision.command.kind == ai::FactionActionKind::DeclareWar &&
            report.decision.command.target == 2 && estimate.has_value() &&
            estimate->military_power < 1600 &&
            world::forecast_region_battle(1000, estimate->military_power,
                                          test_ruleset().combat_rules()) ==
                ai::BattleAssessment::LikelyWin) {
            const auto& combat = test_ruleset().combat_rules();
            const rules::CombatModifiers neutral{combat.modifier_scale, combat.modifier_scale,
                                                 combat.modifier_scale, combat.modifier_scale,
                                                 combat.modifier_scale};
            const auto actual = rules::resolve_region_combat(
                {{1000, neutral, {}, 0}, {1600, neutral, {}, 0},
                 combat.default_exponent, 1},
                combat);
            if (actual.loss_a > actual.loss_b) {
            ++mistaken_losses;
            }
        }
    }
    EXPECT_GT(mistaken_losses, 0U);
    std::cout << "faction_ai mistaken_war_losses_per_1000=" << mistaken_losses << '\n';
}

TEST(FactionAiKnowledge, DistanceAndRelationshipReduceUncertainty) {
    WorldDiplomacyState world{3, 123, test_ruleset()};
    world.set_faction_truth(FactionId{1}, 1000, 1000);
    world.set_faction_truth(FactionId{2}, 1000, 1000);
    world.set_faction_truth(FactionId{3}, 1000, 1000);
    world.observe_faction_by_distance(FactionId{1}, FactionId{2}, 20, 0,
                                      time::Tick{0});
    world.observe_faction_by_distance(FactionId{1}, FactionId{3}, 2, 8000,
                                      time::Tick{0});
    const auto view = world::make_faction_view(world, FactionId{1});
    ASSERT_TRUE(view.estimate(2).has_value());
    ASSERT_TRUE(view.estimate(3).has_value());
    EXPECT_GT(view.estimate(2)->uncertainty_permyriad,
              view.estimate(3)->uncertainty_permyriad);
    std::cout << "faction_ai uncertainty far_hostile="
              << view.estimate(2)->uncertainty_permyriad << " near_friendly="
              << view.estimate(3)->uncertainty_permyriad << '\n';
}

TEST(FactionAiBalance, StrongerHegemonRaisesHostilityUtility) {
    auto world = observed_world(777);
    world.faction_mind(FactionId{1}).forced_goal = ai::FactionGoal::Conquer;
    world.set_faction_truth(FactionId{2}, 800, 600);
    world.observe_faction(FactionId{1}, FactionId{2}, 0, time::Tick{0}, 2);
    const auto before = world::plan_faction_ai_xun(
        world, FactionId{1}, {3, 0, true, true, false}, time::Tick{0},
        test_ruleset());
    world.set_faction_truth(FactionId{2}, 3200, 2400);
    const auto next_xun = time::Tick{static_cast<std::int64_t>(time::kXun)};
    world.observe_faction(FactionId{1}, FactionId{2}, 0, next_xun, 2);
    const auto after = world::plan_faction_ai_xun(
        world, FactionId{1}, {3, 0, true, true, false}, next_xun,
        test_ruleset());
    const auto before_score = utility_for(before.decision, ai::FactionActionKind::DeclareWar, 2);
    const auto after_score = utility_for(after.decision, ai::FactionActionKind::DeclareWar, 2);
    EXPECT_GT(after_score, before_score);
    std::cout << "faction_ai balance_hostility target_power=1400:" << before_score
              << " target_power=5600:" << after_score << '\n';
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

TEST(FactionAiPerformance, AllFactionDecisionsAreMillisecondScaleMinOfFive) {
    auto measure = [] {
        const auto started = std::chrono::steady_clock::now();
        for (std::uint64_t batch = 0; batch < 5; ++batch) {
            auto world = observed_world(9000 + batch);
            for (std::uint16_t faction = 1; faction <= 3; ++faction) {
                static_cast<void>(world::plan_faction_ai_xun(
                    world, FactionId{faction}, {3, 0, true, true, false},
                    time::Tick{0}, test_ruleset()));
            }
        }
        const auto elapsed = std::chrono::steady_clock::now() - started;
        return std::chrono::duration<double, std::milli>(elapsed).count();
    };
    const auto minimum = tests::minimum_milliseconds_after_warmup(measure);
    EXPECT_LT(minimum, 10.0);
    std::cout << "faction_ai perf factions=15 samples="
              << tests::kPerformanceSampleCount << " minimum_ms=" << minimum << '\n';
}

} // namespace

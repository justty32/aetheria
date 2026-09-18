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

}  // namespace

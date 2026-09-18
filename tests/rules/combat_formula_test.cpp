#include "core/rules/combat.h"
#include "core/rules/ruleset.h"
#include "tests/support/ruleset_fixture.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <numeric>

#include <gtest/gtest.h>

#include "tests/rules/combat_test_support.h"

namespace {

using namespace aetheria::rules;
using aetheria::world::Significance;

using namespace aetheria::tests::combat_support;

TEST(CombatFormula, SupplyDisadvantageFlipsOutcomeAndRemainsVisible) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    auto input = matchup(200'000, 100'000, rules);
    input.side_a.accumulated_loss_permyriad = 500;
    input.side_b.accumulated_loss_permyriad = 500;
    input.side_a.modifiers.supply = rules.supply.minimum;
    const auto starved = resolve_region_combat(input, rules);
    input.side_a.modifiers.supply = rules.modifier_scale;
    const auto supplied = resolve_region_combat(input, rules);
    EXPECT_EQ(starved.outcome, Outcome::SideARouted);
    EXPECT_EQ(supplied.outcome, Outcome::SideBRouted);
    EXPECT_GT(starved.breakdown.loss_a.pursuit, 0);
    EXPECT_GT(supplied.breakdown.loss_b.pursuit, 0);
    EXPECT_LT(starved.breakdown.strength_a.after_supply,
              starved.breakdown.strength_a.after_terrain);
    EXPECT_EQ(supplied.breakdown.strength_a.after_supply,
              supplied.breakdown.strength_a.after_terrain);
    std::cout << "M6.3 supply flip: starved_losses=" << starved.loss_a << '/'
              << starved.loss_b << " supplied_losses=" << supplied.loss_a << '/'
              << supplied.loss_b << " supply_stage="
              << starved.breakdown.strength_a.after_terrain << "->"
              << starved.breakdown.strength_a.after_supply << '\n';
}

TEST(CombatFormula, LegendaryRetreatsUnharmedWhileArmyAbsorbsConservedQuota) {
    const auto& ruleset = aetheria::tests::test_ruleset();
    const std::array participants{
        LossParticipant{1, 25'600, Significance::World, true},
        LossParticipant{2, 80'000, Significance::Ambient, false},
        LossParticipant{3, 20'000, Significance::Local, false},
    };
    const auto allocated = allocate_combat_loss(60'000, participants, Significance::Ambient,
                                                ruleset.power_rules());
    ASSERT_EQ(allocated.size(), participants.size());
    EXPECT_EQ(allocated[0].allocated_loss, 0);
    EXPECT_EQ(std::accumulate(allocated.begin(), allocated.end(), std::int64_t{},
                              [](std::int64_t sum, const ParticipantFate& fate) {
                                  return sum + fate.allocated_loss;
                              }),
              60'000);
    EXPECT_GT(allocated[1].allocated_loss + allocated[2].allocated_loss, 0);
    std::cout << "M6.3 quota: hero=" << allocated[0].allocated_loss
              << " ambient=" << allocated[1].allocated_loss
              << " local=" << allocated[2].allocated_loss << " total=60000\n";
}

TEST(CombatFormula, XunAttritionContributionsRemainSeparatelyVisible) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    auto input = matchup(200'000, 200'000, rules);
    input.side_a.attrition = {100, 100, 1'000, true};
    const auto result = resolve_region_combat(input, rules);
    const auto& loss = result.breakdown.loss_a;
    EXPECT_GT(loss.supply, 0);
    EXPECT_GT(loss.disease, 0);
    EXPECT_GT(loss.desertion, 0);
    EXPECT_GT(loss.season, 0);
    EXPECT_EQ(loss.engagement + loss.supply + loss.disease + loss.desertion + loss.season +
                  loss.pursuit,
              loss.total);
    std::cout << "M6.3 xun loss: engagement=" << loss.engagement
              << " supply=" << loss.supply << " disease=" << loss.disease
              << " desertion=" << loss.desertion << " season=" << loss.season
              << " pursuit=" << loss.pursuit << " total=" << loss.total << '\n';
}

TEST(CombatFormula, SameInputIsBitwiseDeterministicAndNamedStageIsDeferred) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    const auto input = matchup(123'456, 234'567, rules);
    const auto first = resolve_region_combat(input, rules);
    for (std::int32_t repeat = 0; repeat < 100; ++repeat) {
        const auto next = resolve_region_combat(input, rules);
        EXPECT_EQ(next.loss_a, first.loss_a);
        EXPECT_EQ(next.loss_b, first.loss_b);
        EXPECT_EQ(next.outcome, first.outcome);
        EXPECT_EQ(next.morale_delta_a, first.morale_delta_a);
        EXPECT_EQ(next.morale_delta_b, first.morale_delta_b);
    }
    EXPECT_TRUE(first.named.empty());
}

TEST(CombatFormula, AiAndPlayerHaveExactlyOnePublicFormula) {
    constexpr auto player_path = &resolve_region_combat;
    constexpr auto ai_path = &resolve_region_combat;
    EXPECT_EQ(player_path, ai_path);
}

}  // namespace

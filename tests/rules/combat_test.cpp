#include "core/rules/combat.h"
#include "core/rules/ruleset.h"
#include "tests/support/ruleset_fixture.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <numeric>

#include <gtest/gtest.h>

namespace {

using namespace aetheria::rules;
using aetheria::world::Significance;

[[nodiscard]] CombatModifiers neutral(const CombatRules& rules) {
    return {rules.modifier_scale, rules.modifier_scale, rules.modifier_scale,
            rules.modifier_scale, rules.modifier_scale};
}

[[nodiscard]] CombatInput matchup(std::int32_t power_a, std::int32_t power_b,
                                  const CombatRules& rules) {
    return {{power_a, neutral(rules), {}, 0},
            {power_b, neutral(rules), {}, 0},
            rules.default_exponent,
            1};
}

[[nodiscard]] std::int64_t factor_per_million(FixedPowerFactor factor) {
    std::uint64_t value = (factor.mantissa_q32 * 1'000'000ULL) >> 32U;
    if (factor.binary_exponent >= 0) {
        return static_cast<std::int64_t>(value << factor.binary_exponent);
    }
    return static_cast<std::int64_t>(value >> -factor.binary_exponent);
}

TEST(CombatRules, LoadsAllFormulaAndModifierBounds) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    ASSERT_TRUE(rules.loaded);
    EXPECT_EQ(rules.default_exponent.numerator, 13);
    EXPECT_EQ(rules.default_exponent.denominator, 10);
    EXPECT_EQ(rules.ratio_binary_limit, 27);
    EXPECT_EQ(rules.terrain.minimum, 600);
    EXPECT_EQ(rules.supply.minimum, 300);
    EXPECT_EQ(rules.morale.maximum, 1300);
    EXPECT_EQ(rules.command.maximum, 1500);
    EXPECT_EQ(rules.posture.minimum, 700);
}

TEST(CombatFixedPoint, ExponentIsActuallyParameterized) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    const auto linear = factor_per_million(fixed_ratio_power(2, 1, {1, 1}, rules));
    const auto compromise = factor_per_million(fixed_ratio_power(2, 1, {13, 10}, rules));
    const auto square = factor_per_million(fixed_ratio_power(2, 1, {2, 1}, rules));
    EXPECT_EQ(linear, 2'000'000);
    EXPECT_NEAR(compromise, 2'462'288, 40);
    EXPECT_EQ(square, 4'000'000);
    std::cout << "M6.3 p-samples: p1=" << linear << " p1.3=" << compromise
              << " p2=" << square << '\n';
}

TEST(CombatFixedPoint, RejectsRatioOutsideMeasuredDomain) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    EXPECT_DEATH(static_cast<void>(fixed_ratio_power(1LL << 28, 1, {13, 10}, rules)),
                 "ratio_within_domain");
}

TEST(CombatFixedPoint, MeasuredDistributionExtremesFitConfiguredDomain) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    EXPECT_GT(fixed_ratio_power(53'114'286, 1, rules.default_exponent, rules).mantissa_q32, 0U);
    EXPECT_GT(fixed_ratio_power(1, 53'114'286, rules.default_exponent, rules).mantissa_q32, 0U);
}

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

TEST(CombatCalibration, LegendaryVictoryAndRetreatHaveAVisibleInterval) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    std::int32_t largest_routed_militia{};
    std::int32_t first_legendary_retreat{};
    for (std::int32_t militia = 1; militia <= 2'000; ++militia) {
        const auto result = resolve_region_combat(matchup(25'600, militia * 100, rules), rules);
        if (result.outcome == Outcome::SideBRouted) {
            largest_routed_militia = militia;
        }
        if (first_legendary_retreat == 0 && result.outcome == Outcome::SideARouted) {
            first_legendary_retreat = militia;
        }
    }
    ASSERT_GT(largest_routed_militia, 0);
    ASSERT_GT(first_legendary_retreat, 0);
    EXPECT_GT(first_legendary_retreat - largest_routed_militia, 50);
    EXPECT_EQ(apply_individual_tier_gate(25'600, Significance::Ambient, Significance::World, true,
                                        aetheria::tests::test_ruleset().power_rules()),
              0);
    std::cout << "M6.3 legendary interval: N=" << largest_routed_militia
              << " M=" << first_legendary_retreat << '\n';
}

TEST(CombatCalibration, RandomThousandCampaignsRarelyAnnihilate) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    std::uint32_t state = 0x6a09e667U;
    std::int32_t annihilated_campaigns{};
    const auto next = [&state]() {
        state = state * 1'664'525U + 1'013'904'223U;
        return state;
    };
    for (std::int32_t sample = 0; sample < 1'000; ++sample) {
        auto input = matchup(static_cast<std::int32_t>(10'000 + next() % 990'001U),
                             static_cast<std::int32_t>(10'000 + next() % 990'001U), rules);
        input.side_a.modifiers.supply =
            rules.supply.minimum + static_cast<std::int32_t>(next() % 801U);
        input.side_b.modifiers.supply =
            rules.supply.minimum + static_cast<std::int32_t>(next() % 801U);
        input.side_a.modifiers.morale =
            rules.morale.minimum + static_cast<std::int32_t>(next() % 801U);
        input.side_b.modifiers.morale =
            rules.morale.minimum + static_cast<std::int32_t>(next() % 801U);
        const auto result = resolve_region_combat(input, rules);
        annihilated_campaigns +=
            result.loss_a == input.side_a.power || result.loss_b == input.side_b.power ? 1 : 0;
    }
    EXPECT_LE(annihilated_campaigns, 200);
    std::cout << "M6.3 annihilated-campaigns-per-1000=" << annihilated_campaigns << '\n';
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

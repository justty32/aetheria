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
    auto input = matchup(120'000, 100'000, rules);
    input.exponent = {1, 1};
    const auto linear_loss = resolve_region_combat(input, rules).loss_b;
    input.exponent = {13, 10};
    const auto compromise_loss = resolve_region_combat(input, rules).loss_b;
    input.exponent = {2, 1};
    const auto square_loss = resolve_region_combat(input, rules).loss_b;
    EXPECT_LT(linear_loss, compromise_loss);
    EXPECT_LT(compromise_loss, square_loss);
    std::cout << "M6.3 p-samples: p1=" << linear << " p1.3=" << compromise
              << " p2=" << square << " marginal_losses=" << linear_loss << '/'
              << compromise_loss << '/' << square_loss << '\n';
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

}  // namespace

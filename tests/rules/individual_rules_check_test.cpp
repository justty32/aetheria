#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

#include "core/rules/attributes.h"
#include "core/rules/check.h"
#include "core/rules/damage.h"
#include "core/rules/ruleset.h"
#include "tests/rules/ruleset_test_support.h"
#include "tests/support/ruleset_fixture.h"

namespace {

using aetheria::rules::Attributes;
using aetheria::rules::DamageResistance;
using aetheria::rules::DamageTypeDef;
using aetheria::rules::DerivedStatModifiers;
using aetheria::rules::RulesetLoader;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::world::Significance;

TEST(IndividualCheck, OneInjectedRngCallPerCheckIsDeterministicAndEveryBandIsHit) {
    const auto& rules = test_ruleset().check_rules();
    constexpr std::uint64_t seed = UINT64_C(0xD100C0DE);
    constexpr std::size_t check_count = 10000;
    std::mt19937_64 rng{seed};
    std::mt19937_64 reference{seed};
    std::array<std::uint32_t, 4> band_hits{};
    std::vector<aetheria::rules::CheckResult> first_sequence;
    first_sequence.reserve(check_count);
    for (std::size_t index = 0; index < check_count; ++index) {
        const auto result = aetheria::rules::perform_check(rng, 70, 0, 0, rules);
        ASSERT_LT(result.band_index, band_hits.size());
        ++band_hits[result.band_index];
        first_sequence.push_back(result);
        const auto expected_roll = static_cast<std::uint8_t>(reference() % UINT64_C(100));
        if (index < 8U) {
            EXPECT_EQ(result.roll, expected_roll) << "決定性序列索引 " << index;
        }
    }
    EXPECT_EQ(rng(), reference()) << "每次檢定必須恰消耗一次 mt19937_64";

    std::mt19937_64 replay{seed};
    for (const auto& expected : first_sequence) {
        EXPECT_EQ(aetheria::rules::perform_check(replay, 70, 0, 0, rules), expected);
    }
    std::cout << "rng_calls=" << check_count << " margin_band_hits=" << band_hits[0] << ','
              << band_hits[1] << ',' << band_hits[2] << ',' << band_hits[3] << '\n';
    for (const auto hits : band_hits) {
        EXPECT_GT(hits, 0U);
    }
    EXPECT_NE(rules.margin_bands.front().effect_percent, rules.margin_bands.back().effect_percent);
}

TEST(IndividualCheck, SuccessBoundaryAndMarginExtremesUseTheSameRoll) {
    const auto& rules = test_ruleset().check_rules();
    const auto failure = aetheria::rules::evaluate_check(50, 50, 0, 0, rules);
    const auto narrow_success = aetheria::rules::evaluate_check(49, 50, 0, 0, rules);
    const auto best = aetheria::rules::evaluate_check(0, 100, 0, 0, rules);
    EXPECT_FALSE(failure.success);
    EXPECT_EQ(failure.margin, 0);
    EXPECT_TRUE(narrow_success.success);
    EXPECT_EQ(narrow_success.margin, 1);
    EXPECT_EQ(failure.effect_percent, 0);
    EXPECT_EQ(best.margin, 100);
    EXPECT_EQ(best.effect_percent, 150);
}

}  // namespace

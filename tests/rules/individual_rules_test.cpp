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

TEST(IndividualAttributes, DerivesEveryValueFromAttributesEquipmentTierAndLoadedFormulas) {
    const auto& rules = test_ruleset().attribute_rules();
    const Attributes attributes{.body = 40, .skill = 50, .mind = 60, .spirit = 70};
    const DerivedStatModifiers equipment{
        .health = 1,
        .mana = 2,
        .accuracy = 3,
        .evasion = 4,
        .defense = 5,
        .resistance = 6,
        .movement = 7,
        .carry_capacity = 8,
        .vision = 9,
    };
    const auto derived =
        aetheria::rules::derive_stats(attributes, Significance::Site, equipment, rules);
    EXPECT_EQ(derived.health, 311);
    EXPECT_EQ(derived.mana, 408);
    EXPECT_EQ(derived.accuracy, 61);
    EXPECT_EQ(derived.evasion, 58);
    EXPECT_EQ(derived.defense, 50);
    EXPECT_EQ(derived.resistance, 67);
    EXPECT_EQ(derived.movement, 13);
    EXPECT_EQ(derived.carry_capacity, 102);
    EXPECT_EQ(derived.vision, 15);
}

TEST(IndividualAttributes, ThousandRandomDefaultsStayCalibratedAndCoverAllFiveTiers) {
    const auto& rules = test_ruleset().attribute_rules();
    std::mt19937_64 rng{UINT64_C(0xA37E71A)};
    std::array<std::uint32_t, 5> tier_hits{};
    std::uint32_t excessive_difference{};
    std::uint8_t maximum_difference{};
    for (std::size_t sample = 0; sample < 1000; ++sample) {
        const Attributes attributes{
            .body = static_cast<std::int32_t>(rng() % 101U),
            .skill = static_cast<std::int32_t>(rng() % 101U),
            .mind = static_cast<std::int32_t>(rng() % 101U),
            .spirit = static_cast<std::int32_t>(rng() % 101U),
        };
        const auto suggested = aetheria::rules::suggest_tier(attributes, rules);
        const auto actual = aetheria::rules::resolve_tier(attributes, rules);
        const auto suggested_value = static_cast<std::uint8_t>(suggested);
        const auto actual_value = static_cast<std::uint8_t>(actual);
        const auto difference = static_cast<std::uint8_t>(suggested_value > actual_value
                                                              ? suggested_value - actual_value
                                                              : actual_value - suggested_value);
        excessive_difference += difference > 1U ? 1U : 0U;
        maximum_difference = std::max(maximum_difference, difference);
        ++tier_hits[actual_value];
    }
    std::cout << "tier_hits=" << tier_hits[0] << ',' << tier_hits[1] << ',' << tier_hits[2] << ','
              << tier_hits[3] << ',' << tier_hits[4]
              << " excessive_difference=" << excessive_difference
              << " maximum_difference=" << static_cast<unsigned>(maximum_difference) << '\n';
    EXPECT_EQ(excessive_difference, 0U);
    EXPECT_EQ(maximum_difference, 0U);
    for (const auto hits : tier_hits) {
        EXPECT_GT(hits, 0U);
        EXPECT_LT(hits, 900U);
    }
}

TEST(IndividualAttributes, TierOverrideRequiresAnExplicitReason) {
    const auto& rules = test_ruleset().attribute_rules();
    const Attributes attributes{.body = 10, .skill = 10, .mind = 10, .spirit = 10};
    EXPECT_EQ(aetheria::rules::resolve_tier(
                  attributes, rules,
                  aetheria::rules::TierOverride{.tier = Significance::World, .reason = "持有神器"}),
              Significance::World);
    EXPECT_THROW((void)aetheria::rules::resolve_tier(
                     attributes, rules,
                     aetheria::rules::TierOverride{.tier = Significance::World, .reason = ""}),
                 std::invalid_argument);
}

}  // namespace

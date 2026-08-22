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

static_assert(!std::is_enum_v<DamageTypeDef>);

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

TEST(IndividualDamage, EveryLoadedDefinitionHitsAndSparseResistanceCanBeNegative) {
    const auto& ruleset = test_ruleset();
    ASSERT_EQ(ruleset.damage_types().size(), 9U);
    std::vector<DamageResistance> resistances;
    resistances.reserve(ruleset.damage_types().size());
    for (std::size_t index = 0; index < ruleset.damage_types().size(); ++index) {
        resistances.push_back({.type = static_cast<aetheria::rules::DamageTypeId>(index),
                               .percent = index == 0 ? -25 : 10});
    }
    std::cout << "damage_type_hits=";
    for (std::size_t index = 0; index < ruleset.damage_types().size(); ++index) {
        const auto id = static_cast<aetheria::rules::DamageTypeId>(index);
        const auto result =
            aetheria::rules::apply_resistance(100, id, resistances, ruleset.damage_rules());
        EXPECT_EQ(result.actual_damage, index == 0 ? 125 : 90);
        EXPECT_EQ(ruleset.damage_type(id), &ruleset.damage_types()[index]);
        std::cout << (index == 0 ? "" : ",") << ruleset.damage_types()[index].id << "=1";
    }
    std::cout << '\n';
}

TEST(IndividualDamage, ExtremeResistanceIsCappedBelowCompleteImmunity) {
    const auto& ruleset = test_ruleset();
    const auto type = *ruleset.find_damage_type("damage.slash");
    const std::array resistances{DamageResistance{.type = type, .percent = 1'000'000}};
    const auto result =
        aetheria::rules::apply_resistance(100, type, resistances, ruleset.damage_rules());
    EXPECT_EQ(result.applied_resistance_percent, 90);
    EXPECT_EQ(result.actual_damage, 10);
    EXPECT_EQ(aetheria::rules::apply_resistance(1, type, resistances, ruleset.damage_rules())
                  .actual_damage,
              1);
}

TEST(IndividualDamage, AddingATypeRequiresOnlyEditingDamageToml) {
    TemporaryDirectory directory;
    for (const auto& entry : std::filesystem::directory_iterator{AETHERIA_SOURCE_DIR "/data"}) {
        if (entry.is_regular_file()) {
            std::filesystem::copy_file(entry.path(), directory.path() / entry.path().filename());
        }
    }
    std::ofstream damage{directory.path() / "damage.toml", std::ios::app};
    damage << R"toml(
[[defs]]
id = "damage.arcane"
name_key = "damage.arcane"
category = "supernatural"
)toml";
    damage.close();
    const auto extended = RulesetLoader::load(directory.path());
    EXPECT_EQ(extended.damage_types().size(), test_ruleset().damage_types().size() + 1U);
    const auto arcane = extended.find_damage_type("damage.arcane");
    ASSERT_TRUE(arcane.has_value());
    EXPECT_EQ(extended.damage_type(*arcane)->category, "supernatural");
}

}  // namespace

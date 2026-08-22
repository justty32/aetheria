#include "core/rules/power.h"
#include "core/rules/ruleset.h"
#include "tests/support/ruleset_fixture.h"

#include <array>
#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::PowerRules;
using aetheria::rules::PowerStack;
using aetheria::world::Significance;

[[nodiscard]] std::int64_t score(const PowerRules& rules, std::int64_t count,
                                 Significance tier, std::int32_t quality_percent) {
    const std::array stacks{PowerStack{count, tier, quality_percent}};
    return aetheria::rules::equivalent_power(stacks, rules);
}

TEST(PowerRules, LoadsEveryTierAndBreakthroughDefinitions) {
    const auto& ruleset = aetheria::tests::test_ruleset();
    const auto& rules = ruleset.power_rules();
    ASSERT_TRUE(rules.loaded);

    constexpr std::array tiers{Significance::Ambient, Significance::Local, Significance::Site,
                               Significance::Region, Significance::World};
    std::array<std::uint32_t, aetheria::rules::kPowerTierCount> hit_counts{};
    for (const auto tier : tiers) {
        const auto index = static_cast<std::size_t>(tier);
        ++hit_counts[index];
        EXPECT_EQ(score(rules, 1, tier, rules.reference_quality_percent),
                  rules.tier_weights[index] * rules.reference_quality_percent);
    }
    EXPECT_EQ(hit_counts, (decltype(hit_counts){1U, 1U, 1U, 1U, 1U}));

    ASSERT_FALSE(ruleset.breakthroughs().empty());
    for (const auto& def : ruleset.breakthroughs()) {
        const auto id = ruleset.find_breakthrough(def.id);
        ASSERT_TRUE(id.has_value()) << def.id;
        EXPECT_EQ(ruleset.breakthrough(*id), &def);
    }
}

TEST(PowerCalibration, LegendaryBeatsAGroupButLosesToAnArmy) {
    const auto& rules = aetheria::tests::test_ruleset().power_rules();
    const auto quality = rules.reference_quality_percent;
    const auto legendary = score(rules, 1, Significance::World, quality);
    const auto one_ambient = score(rules, 1, Significance::Ambient, quality);

    EXPECT_GT(legendary, score(rules, 100, Significance::Ambient, quality));
    EXPECT_LT(legendary, score(rules, 1'000, Significance::Ambient, quality));

    ASSERT_GT(one_ambient, 0);
    const auto tie_count = legendary / one_ambient;
    ASSERT_GT(tie_count, 0);
    EXPECT_GT(legendary, score(rules, tie_count - 1, Significance::Ambient, quality));
    EXPECT_EQ(legendary, score(rules, tie_count, Significance::Ambient, quality));
    EXPECT_LT(legendary, score(rules, tie_count + 1, Significance::Ambient, quality));
}

TEST(PowerCalibration, QuantityNeverPromotesCohortTier) {
    const auto& rules = aetheria::tests::test_ruleset().power_rules();
    const std::array cohort{
        PowerStack{1'000, Significance::Ambient, rules.reference_quality_percent}};
    EXPECT_EQ(aetheria::rules::cohort_tier(cohort), Significance::Ambient);
    EXPECT_GT(aetheria::rules::equivalent_power(cohort, rules), 0);
}

TEST(PowerCalibration, AdjacentQualityExtremesTouchExactly) {
    const auto& rules = aetheria::tests::test_ruleset().power_rules();
    constexpr std::array tiers{Significance::Ambient, Significance::Local, Significance::Site,
                               Significance::Region, Significance::World};
    for (std::size_t index = 0; index + 1 < tiers.size(); ++index) {
        EXPECT_EQ(score(rules, 1, tiers[index], rules.maximum_quality_percent),
                  score(rules, 1, tiers[index + 1], rules.minimum_quality_percent));
    }
}

TEST(PowerTierGate, ProtectsOnlyIndividualsAndBreakthroughRestoresDamage) {
    const auto& ruleset = aetheria::tests::test_ruleset();
    const auto& rules = ruleset.power_rules();
    const auto ambient_army_damage =
        score(rules, 10'000, Significance::Ambient, rules.reference_quality_percent);
    const auto local_army_damage =
        score(rules, 10'000, Significance::Local, rules.reference_quality_percent);
    const auto site_army_damage =
        score(rules, 10'000, Significance::Site, rules.reference_quality_percent);

    EXPECT_EQ(aetheria::rules::apply_individual_tier_gate(
                  ambient_army_damage, Significance::Ambient, Significance::World, true, rules),
              0);
    EXPECT_EQ(aetheria::rules::apply_individual_tier_gate(
                  local_army_damage, Significance::Local, Significance::World, true, rules),
              0);
    EXPECT_EQ(aetheria::rules::apply_individual_tier_gate(
                  site_army_damage, Significance::Site, Significance::World, true, rules),
              site_army_damage);
    EXPECT_EQ(aetheria::rules::apply_individual_tier_gate(
                  ambient_army_damage, Significance::Ambient, Significance::World, false, rules),
              ambient_army_damage);

    const auto artifact_id = ruleset.find_breakthrough("breakthrough.artifact");
    ASSERT_TRUE(artifact_id.has_value());
    const auto* artifact = ruleset.breakthrough(*artifact_id);
    ASSERT_NE(artifact, nullptr);
    EXPECT_EQ(aetheria::rules::apply_individual_tier_gate(
                  ambient_army_damage, Significance::Ambient, Significance::World, true, rules,
                  artifact),
              ambient_army_damage);
}

TEST(PowerRules, EquivalentPowerIsDeterministicAndRejectsOverflow) {
    const auto& rules = aetheria::tests::test_ruleset().power_rules();
    const std::array stacks{
        PowerStack{37, Significance::Local, rules.reference_quality_percent},
        PowerStack{11, Significance::Region, rules.minimum_quality_percent}};
    const auto first = aetheria::rules::equivalent_power(stacks, rules);
    for (std::size_t repetition = 0; repetition < 100U; ++repetition) {
        EXPECT_EQ(aetheria::rules::equivalent_power(stacks, rules), first);
    }

    const std::array overflowing{PowerStack{std::numeric_limits<std::int64_t>::max(),
                                            Significance::World,
                                            rules.maximum_quality_percent}};
    EXPECT_DEATH(static_cast<void>(aetheria::rules::equivalent_power(overflowing, rules)),
                 "AETH_CHECK failed");
}

}  // namespace

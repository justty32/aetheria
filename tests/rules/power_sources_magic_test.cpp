#include "core/rules/combat.h"
#include "core/rules/power.h"
#include "core/rules/power_sources.h"
#include "core/rules/ruleset.h"
#include "tests/rules/ruleset_test_support.h"
#include "tests/support/ruleset_fixture.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

using namespace aetheria::rules;
using aetheria::world::Significance;

[[nodiscard]] CombatModifiers neutral(const CombatRules& rules) {
    return {rules.modifier_scale, rules.modifier_scale, rules.modifier_scale,
            rules.modifier_scale, rules.modifier_scale};
}

[[nodiscard]] CombatInput matchup(const CombatRules& rules) {
    return {{100'000, neutral(rules), {}, 0},
            {100'000, neutral(rules), {}, 0},
            rules.default_exponent,
            1};
}

TEST(MagicLayers, RegionRitualIsSlowExpensiveAndChangesTheExistingCombatFormula) {
    const auto& ruleset = aetheria::tests::test_ruleset();
    const auto& source_rules = ruleset.power_source_rules();
    RegionMagicState region{
        .magical_resource = 120,
        .resource_income_per_xun = 20,
        .weather_modifier = 0,
        .combat_posture_modifier = 0,
        .cooldown_remaining_xun = 0,
        .rituals = {},
    };
    const StrategicSpellDef spell{
        .id = "spell.test.region",
        .scale = SpellScale::Region,
        .magical_resource_cost = 120,
        .ritual_duration_xun = 3,
        .required_casters = 3,
        .cooldown_xun = 4,
        .weather_delta = -15,
        .combat_posture_delta = 300,
    };
    EXPECT_TRUE(begin_strategic_ritual(region, spell, 3, source_rules));
    EXPECT_EQ(region.magical_resource, 0);
    EXPECT_FALSE(begin_strategic_ritual(region, spell, 3, source_rules));
    advance_region_magic(region, 2);
    EXPECT_EQ(region.weather_modifier, 0);
    advance_region_magic(region, 1);
    EXPECT_EQ(region.weather_modifier, -15);
    EXPECT_EQ(region.combat_posture_modifier, 300);
    EXPECT_EQ(region.cooldown_remaining_xun, 4);

    const auto& combat_rules = ruleset.combat_rules();
    const auto baseline = resolve_region_combat(matchup(combat_rules), combat_rules);
    auto enchanted_input = matchup(combat_rules);
    enchanted_input.side_a.modifiers.posture += region.combat_posture_modifier;
    const auto enchanted = resolve_region_combat(enchanted_input, combat_rules);
    EXPECT_GT(enchanted.breakdown.strength_a.adjusted,
              baseline.breakdown.strength_a.adjusted);

    advance_region_magic(region, 4);
    EXPECT_EQ(region.cooldown_remaining_xun, 0);
    EXPECT_EQ(region.magical_resource, 140);
    EXPECT_TRUE(begin_strategic_ritual(region, spell, 3, source_rules));
    constexpr auto ten_xun_per_turn_demand = 10 * 120;
    constexpr auto ten_xun_available = 120 + 10 * 20;
    EXPECT_EQ(ten_xun_per_turn_demand, 1'200);
    EXPECT_EQ(ten_xun_available, 320);
    EXPECT_LT(ten_xun_available, ten_xun_per_turn_demand);
    std::cout << "strategic_magic cost=120 ritual=3 cooldown=4 casters=3 income=20 "
                 "ten_xun_available=320 ten_xun_every_turn_demand=1200 deficit=880\n";
}

TEST(MagicLayers, SiteSpellChangesFormationAndLocalSpellUsesM6D100WithBacklash) {
    std::int32_t formation_mana = 80;
    FormationMagicState formation;
    const TacticalSpellDef tactical{
        .id = "spell.test.site",
        .mana_cost = 30,
        .attack_modifier = 120,
        .defense_modifier = 80,
        .summoned_mass = 24,
    };
    EXPECT_TRUE(cast_tactical_spell(formation_mana, formation, tactical));
    EXPECT_EQ(formation_mana, 50);
    EXPECT_EQ(formation.attack_modifier, 120);
    EXPECT_EQ(formation.defense_modifier, 80);
    EXPECT_EQ(formation.summoned_mass, 24);

    IndividualCasterState failed{.mind = 60, .mana = 100, .health = 50};
    const IndividualSpellDef local{
        .id = "spell.test.local",
        .mana_cost = 20,
        .difficulty = 10,
        .modifier = 0,
        .effect_value = 100,
        .failure_backlash = 7,
    };
    const auto failure = cast_individual_spell(
        failed, local, 75, aetheria::tests::test_ruleset().check_rules());
    EXPECT_FALSE(failure.check.success);
    EXPECT_EQ(failure.check.roll, 75);
    EXPECT_EQ(failed.mana, 80);
    EXPECT_EQ(failed.health, 43);
    EXPECT_EQ(failure.backlash_damage, 7);

    IndividualCasterState succeeded{.mind = 60, .mana = 100, .health = 50};
    const auto success = cast_individual_spell(
        succeeded, local, 10, aetheria::tests::test_ruleset().check_rules());
    EXPECT_TRUE(success.check.success);
    EXPECT_EQ(success.check.roll, 10);
    EXPECT_EQ(success.applied_effect, 150);
    EXPECT_EQ(succeeded.mana, 80);
    EXPECT_EQ(succeeded.health, 50);
}

TEST(MagicFailureCost, RemovingConsequencesMakesRetryingMonotonicallyBetter) {
    constexpr std::array attempts{1U, 2U, 5U, 10U};
    std::array<std::uint32_t, attempts.size()> probabilities{};
    for (std::size_t index = 0; index < attempts.size(); ++index) {
        probabilities[index] = retry_success_permyriad(60, 0, 10, attempts[index]);
    }
    EXPECT_EQ(probabilities, (std::array<std::uint32_t, 4>{5'000, 7'500, 9'687, 9'990}));
    for (std::size_t index = 1; index < probabilities.size(); ++index) {
        EXPECT_GT(probabilities[index], probabilities[index - 1]);
    }
    std::cout << "costless_retry_success_permyriad=5000,7500,9687,9990 "
                 "attempts=1,2,5,10\n";
}

}  // namespace

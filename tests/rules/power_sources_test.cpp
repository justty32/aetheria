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

[[nodiscard]] PowerSourceDef source(PowerSourceKind kind, std::string id,
                                    std::int8_t bonus, std::string ability,
                                    Attributes modifier = {}) {
    std::vector<std::string> abilities;
    if (!ability.empty()) {
        abilities.push_back(std::move(ability));
    }
    return {
        .kind = kind,
        .origin_id = std::move(id),
        .tier_bonus = bonus,
        .ability_ids = std::move(abilities),
        .attribute_modifier = modifier,
        .removal_condition = kind == PowerSourceKind::Bloodline
                                 ? "condition.bloodline_lost"
                                 : "condition.source_deprived",
    };
}

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

TEST(PowerSourceLoader, LoadsSixSchoolSkeletonsAndStrategicFloorsWithoutInventingContent) {
    const auto& ruleset = aetheria::tests::test_ruleset();
    constexpr std::array expected{
        "school.elemental", "school.summoning", "school.illusion",
        "school.necromancy", "school.transmutation", "school.divination"};
    ASSERT_EQ(ruleset.schools().size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const auto id = ruleset.find_school(expected[index]);
        ASSERT_TRUE(id.has_value());
        EXPECT_EQ(ruleset.school(*id), &ruleset.schools()[index]);
        EXPECT_EQ(ruleset.schools()[index].source.kind, PowerSourceKind::Magic);
    }
    EXPECT_TRUE(ruleset.tenets().empty());
    EXPECT_TRUE(ruleset.deities().empty());
    EXPECT_TRUE(ruleset.races().empty());
    const auto& rules = ruleset.power_source_rules();
    EXPECT_TRUE(rules.loaded);
    EXPECT_EQ(rules.strategic_minimum_resource_cost, 120);
    EXPECT_EQ(rules.strategic_minimum_ritual_xun, 3);
    EXPECT_EQ(rules.strategic_minimum_casters, 3);
    EXPECT_EQ(rules.strategic_minimum_cooldown_xun, 4);
    EXPECT_EQ(rules.strategic_reference_resource_income_per_xun, 20);
}

TEST(PowerSourceLoader, SyntheticDefinitionsExerciseTenetDeityAndRaceSchemas) {
    aetheria::tests::TemporaryDirectory directory;
    std::filesystem::copy(AETHERIA_SOURCE_DIR "/data", directory.path(),
                          std::filesystem::copy_options::recursive |
                              std::filesystem::copy_options::overwrite_existing);
    const auto path = directory.path() / "power_sources.toml";
    std::ifstream input{path};
    ASSERT_TRUE(input);
    std::string text{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    const auto replace = [&](std::string_view before, std::string_view after) {
        const auto position = text.find(before);
        ASSERT_NE(position, std::string::npos);
        text.replace(position, before.size(), after);
    };
    replace("tenets = []", R"toml([[tenets]]
id = "tenet.test"
name_key = "tenet.test.name"
conflicts = []
)toml");
    replace("deities = []", R"toml([[deities]]
id = "deity.test"
name_key = "deity.test.name"
significance = "world"
domains = ["domain.test"]
tenets = ["tenet.test"]
hostile_deities = []
source_tier_bonus = 2
source_abilities = ["ability.faith.test"]
source_attribute_modifier = [0, 0, 0, 5]
source_removal_condition = "condition.deity_fallen"
)toml");
    replace("races = []", R"toml([[races]]
id = "race.test"
name_key = "race.test.name"
tier_cap = "region"
lifespan_years = 80
source_tier_bonus = 0
source_abilities = ["ability.race.test"]
source_attribute_modifier = [5, 0, 0, 0]
source_removal_condition = "condition.bloodline_lost"
)toml");
    std::ofstream output{path, std::ios::trunc};
    ASSERT_TRUE(output);
    output << text;
    output.close();

    const auto ruleset = RulesetLoader::load(directory.path());
    ASSERT_EQ(ruleset.tenets().size(), 1U);
    ASSERT_EQ(ruleset.deities().size(), 1U);
    ASSERT_EQ(ruleset.races().size(), 1U);
    EXPECT_EQ(ruleset.deity(*ruleset.find_deity("deity.test"))->significance,
              Significance::World);
    EXPECT_EQ(ruleset.deity(*ruleset.find_deity("deity.test"))->tenets.front(),
              "tenet.test");
    EXPECT_EQ(ruleset.race(*ruleset.find_race("race.test"))->tier_cap,
              Significance::Region);
    EXPECT_EQ(ruleset.race(*ruleset.find_race("race.test"))->lifespan_years, 80U);
}

TEST(PowerSourceConnector, MagicUsesTheSharedTierAbilityAndAttributePath) {
    const auto magic = source(PowerSourceKind::Magic, "school.test", 2,
                              "ability.magic.local", {.mind = 7});
    const std::array states{PowerSourceState{&magic, true, {}}};
    const auto profile = resolve_power_profile(Significance::Ambient, Significance::World,
                                               {.mind = 40}, states);
    EXPECT_EQ(profile.tier, Significance::Site);
    EXPECT_EQ(profile.attributes.mind, 47);
    EXPECT_EQ(profile.ability_ids, std::vector<std::string>{"ability.magic.local"});
}

TEST(PowerSourceConnector, FaithUsesTheSharedTierAbilityAndAttributePath) {
    const auto faith = source(PowerSourceKind::Faith, "deity.test", 2,
                              "ability.faith.domain", {.spirit = 9});
    const std::array states{PowerSourceState{&faith, true, {}}};
    const auto profile = resolve_power_profile(Significance::Local, Significance::World,
                                               {.spirit = 30}, states);
    EXPECT_EQ(profile.tier, Significance::Region);
    EXPECT_EQ(profile.attributes.spirit, 39);
    EXPECT_EQ(profile.ability_ids, std::vector<std::string>{"ability.faith.domain"});
}

TEST(PowerSourceConnector, BloodlineUsesTheSharedPathAndEnforcesItsCap) {
    const auto bloodline = source(PowerSourceKind::Bloodline, "race.test", 0,
                                  "ability.race.innate", {.body = 6});
    const std::array states{PowerSourceState{&bloodline, true, {}}};
    const auto profile = resolve_power_profile(Significance::Site, Significance::Local,
                                               {.body = 20}, states);
    EXPECT_EQ(profile.tier, Significance::Local);
    EXPECT_EQ(profile.attributes.body, 26);
    EXPECT_EQ(profile.ability_ids, std::vector<std::string>{"ability.race.innate"});
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

TEST(FaithRules, ConflictingTenetsHaveAVisibleCostAndViolationImmediatelyDeprives) {
    const std::array tenets{
        TenetDef{"tenet.war", "tenet.war.name", {"tenet.peace"}},
        TenetDef{"tenet.peace", "tenet.peace.name", {"tenet.war"}},
    };
    const std::array selected{std::string{"tenet.war"}, std::string{"tenet.peace"}};
    EXPECT_EQ(count_tenet_conflicts(selected, tenets), 1U);

    const auto faith = source(PowerSourceKind::Faith, "deity.test", 2,
                              "ability.faith.test");
    FaithFollower follower{
        .entity_id = 1,
        .base_tier = Significance::Local,
        .tier_cap = Significance::World,
        .base_attributes = {},
        .sources = {PowerSourceState{&faith, true, {}}},
    };
    EXPECT_EQ(resolve_follower(follower).tier, Significance::Region);
    EXPECT_EQ(deprive_faith(follower, "deity.test", "tenet_violated"), 2U);
    EXPECT_EQ(resolve_follower(follower).tier, Significance::Local);
    EXPECT_EQ(follower.sources.front().unavailable_reason, "tenet_violated");
}

TEST(FaithRules, WorldDeityFallDropsEveryFollowerThroughOneIntermediaryEvent) {
    const auto faith = source(PowerSourceKind::Faith, "deity.test", 2,
                              "ability.faith.test");
    const DeityDef deity{
        .id = "deity.test",
        .name_key = "deity.test.name",
        .significance = Significance::World,
        .domains = {},
        .tenets = {},
        .hostile_deities = {},
        .source = faith,
    };
    RootFaithState root{{RootDeityState{&deity, true}}};
    std::vector<FaithFollower> followers;
    for (std::uint64_t id = 1; id <= 12; ++id) {
        followers.push_back({
            .entity_id = id,
            .base_tier = Significance::Local,
            .tier_cap = Significance::World,
            .base_attributes = {},
            .sources = {PowerSourceState{&faith, true, {}}},
        });
    }
    const auto before = equivalent_power(
        std::array{PowerStack{12, Significance::Region, 100}},
        aetheria::tests::test_ruleset().power_rules());
    const auto result = fall_deity(root, "deity.test", followers);
    const auto after = equivalent_power(
        std::array{PowerStack{12, Significance::Local, 100}},
        aetheria::tests::test_ruleset().power_rules());
    EXPECT_EQ(result.affected_followers, 12U);
    EXPECT_EQ(result.total_tiers_lost, 24U);
    EXPECT_EQ(result.event.follower_ids.size(), 12U);
    EXPECT_FALSE(root.deities.front().alive);
    EXPECT_EQ(before, 76'800);
    EXPECT_EQ(after, 4'800);
    for (const auto& follower : followers) {
        EXPECT_EQ(resolve_follower(follower).tier, Significance::Local);
    }
    std::cout << "deity_fall affected=12 tier_drop_each=2 total_tiers_lost=24 "
                 "equivalent_power=76800->4800\n";
}

TEST(RaceRules, LocalCapBlocksSiteDespiteEveryOtherSourceAndLifespanIsNotTheCap) {
    const auto magic = source(PowerSourceKind::Magic, "school.test", 2, "ability.magic");
    const auto faith = source(PowerSourceKind::Faith, "deity.test", 2, "ability.faith");
    const auto bloodline = source(PowerSourceKind::Bloodline, "race.beast", 0, "ability.race");
    const std::array all_sources{
        PowerSourceState{&magic, true, {}}, PowerSourceState{&faith, true, {}},
        PowerSourceState{&bloodline, true, {}}};
    const auto forced = resolve_power_profile(Significance::Ambient, Significance::Local,
                                              {}, all_sources);
    EXPECT_EQ(forced.tier, Significance::Local);
    EXPECT_NE(forced.tier, Significance::Site);

    const RaceDef human{
        .id = "race.human",
        .name_key = "race.human.name",
        .source = source(PowerSourceKind::Bloodline, "race.human", 0, ""),
        .tier_cap = Significance::Region,
        .lifespan_years = 80,
    };
    const RaceDef elf{
        .id = "race.elf",
        .name_key = "race.elf.name",
        .source = source(PowerSourceKind::Bloodline, "race.elf", 0, ""),
        .tier_cap = Significance::Region,
        .lifespan_years = 500,
    };
    EXPECT_TRUE(can_reach_within_lifespan(human, Significance::Region, 40));
    EXPECT_TRUE(can_reach_within_lifespan(elf, Significance::Region, 400));
    EXPECT_FALSE(can_reach_within_lifespan(human, Significance::Region, 400));
    EXPECT_FALSE(can_reach_within_lifespan(elf, Significance::World, 400));
    std::cout << "race_cap attempted=site actual=local human_genius_years=40 "
                 "elf_typical_years=400 shared_cap=region\n";
}

TEST(NonSubstitutability, EachSourceRetainsAnOperationTheOtherTwoCannotSupply) {
    const auto magic = source(PowerSourceKind::Magic, "school.test", 2, "ability.magic");
    const auto faith = source(PowerSourceKind::Faith, "deity.test", 2, "ability.faith");
    const auto bloodline = source(PowerSourceKind::Bloodline, "race.test", 0, "ability.race");
    const auto magic_profile = resolve_power_profile(
        Significance::Ambient, Significance::World, {},
        std::array{PowerSourceState{&magic, true, {}}});
    const auto faith_profile = resolve_power_profile(
        Significance::Ambient, Significance::World, {},
        std::array{PowerSourceState{&faith, true, {}}});
    const auto bloodline_profile = resolve_power_profile(
        Significance::Local, Significance::Region, {},
        std::array{PowerSourceState{&bloodline, true, {}}});
    const std::array battle_stacks{
        PowerStack{1, magic_profile.tier, 100}, PowerStack{1, faith_profile.tier, 100},
        PowerStack{1, bloodline_profile.tier, 100}};
    EXPECT_EQ(equivalent_power(battle_stacks,
                               aetheria::tests::test_ruleset().power_rules()),
              3'600);

    // 魔法：完成跨旬儀式後才有 Region 戰鬥修正。
    RegionMagicState region{
        .magical_resource = 120,
        .resource_income_per_xun = 20,
        .weather_modifier = 0,
        .combat_posture_modifier = 0,
        .cooldown_remaining_xun = 0,
        .rituals = {},
    };
    const StrategicSpellDef ritual{
        .id = "spell.test.turning_point",
        .magical_resource_cost = 120,
        .ritual_duration_xun = 3,
        .required_casters = 3,
        .cooldown_xun = 4,
        .combat_posture_delta = 300,
    };
    EXPECT_TRUE(begin_strategic_ritual(region, ritual, 3,
                                      aetheria::tests::test_ruleset().power_source_rules()));
    advance_region_magic(region, 3);
    EXPECT_EQ(region.combat_posture_modifier, 300);

    // 信仰：同一個人可以被 World 事件立刻剝奪兩階。
    FaithFollower follower{1, Significance::Local, Significance::World, {},
                           {PowerSourceState{&faith, true, {}}}};
    EXPECT_EQ(deprive_faith(follower, "deity.test", "world_event"), 2U);

    // 血統：即使魔法與信仰合計 +4，Local 上限仍拒絕 Site。
    const std::array boosts{PowerSourceState{&magic, true, {}},
                            PowerSourceState{&faith, true, {}}};
    EXPECT_EQ(resolve_power_profile(Significance::Ambient, Significance::Local, {}, boosts).tier,
              Significance::Local);
    std::cout << "same_battle_S magic=1600 faith=1600 bloodline=400 total=3600\n";
}

}  // namespace

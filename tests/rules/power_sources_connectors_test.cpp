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

#include "tests/rules/power_sources_test_support.h"

namespace {

using namespace aetheria::rules;
using aetheria::world::Significance;

using namespace aetheria::tests::power_sources_support;

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

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

}  // namespace

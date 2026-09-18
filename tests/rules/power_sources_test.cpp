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

}  // namespace

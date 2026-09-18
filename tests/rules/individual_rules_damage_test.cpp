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

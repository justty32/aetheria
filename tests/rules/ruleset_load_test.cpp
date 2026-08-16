#include "core/rules/ruleset.h"
#include "tests/support/ruleset_fixture.h"

#include <concepts>
#include <cstdint>
#include <span>
#include <type_traits>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::Ruleset;
using aetheria::rules::TerrainDef;
using aetheria::tests::test_ruleset;

static_assert(std::same_as<decltype(std::declval<const Ruleset&>().terrains()),
                           std::span<const TerrainDef>>);
static_assert(!std::is_assignable_v<
              decltype((std::declval<const Ruleset&>().terrains()[0].move_cost)), std::int32_t>);

TEST(RulesetLoader, LoadsFourImmutableDefinitionTypes) {
    const auto& ruleset = test_ruleset();
    ASSERT_EQ(ruleset.terrains().size(), 5U);
    ASSERT_EQ(ruleset.reliefs().size(), 3U);
    ASSERT_EQ(ruleset.features().size(), 5U);
    ASSERT_EQ(ruleset.edges().size(), 17U);
    ASSERT_EQ(ruleset.biome_rules().size(), 6U);
    EXPECT_TRUE(ruleset.biome_rules().back().fallback);
    EXPECT_TRUE(ruleset.movement_rules().loaded);
    EXPECT_TRUE(ruleset.civilization_rules().loaded);
    EXPECT_EQ(ruleset.terrain(*ruleset.find_terrain("terrain.grassland"))->move_cost, 1);
    EXPECT_EQ(ruleset.feature(*ruleset.find_feature("feature.forest"))->required_terrain,
              ruleset.find_terrain("terrain.grassland"));
}

}  // namespace

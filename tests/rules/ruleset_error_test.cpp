#include "core/rules/ruleset.h"
#include "tests/rules/ruleset_test_support.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::RulesetLoader;
using aetheria::tests::kGrass;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::write_text;
using aetheria::tests::write_valid_ruleset;

void copy_default_ruleset(const std::filesystem::path& destination) {
    std::filesystem::copy(AETHERIA_SOURCE_DIR "/data", destination,
                          std::filesystem::copy_options::recursive |
                              std::filesystem::copy_options::overwrite_existing);
}

[[nodiscard]] std::string read_text(const std::filesystem::path& path) {
    std::ifstream stream{path};
    if (!stream) {
        throw std::runtime_error{"Cannot read Ruleset test file"};
    }
    return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

void replace_once(std::string& text, std::string_view before, std::string_view after) {
    const auto position = text.find(before);
    if (position == std::string::npos) {
        throw std::runtime_error{"Ruleset test fixture text not found"};
    }
    text.replace(position, before.size(), after);
}

TEST(RulesetLoader, RejectsMissingFile) {
    TemporaryDirectory directory;
    EXPECT_THROW(static_cast<void>(RulesetLoader::load(directory.path())), std::runtime_error);
}

TEST(RulesetLoader, RejectsMalformedToml) {
    TemporaryDirectory directory;
    write_text(directory.path() / "terrain.toml", "[[defs]\nid='broken'");
    EXPECT_THROW(static_cast<void>(RulesetLoader::load(directory.path())), std::runtime_error);
}

TEST(RulesetLoader, RejectsMissingDefsSection) {
    TemporaryDirectory directory;
    write_text(directory.path() / "terrain.toml", "title='missing defs'\n");
    EXPECT_THROW(static_cast<void>(RulesetLoader::load(directory.path())), std::runtime_error);
}

TEST(RulesetLoader, RejectsGloballyDuplicateId) {
    TemporaryDirectory directory;
    write_valid_ruleset(directory.path(), std::string{kGrass} + std::string{kGrass});
    EXPECT_THROW(static_cast<void>(RulesetLoader::load(directory.path())), std::runtime_error);
}

TEST(RulesetLoader, RejectsIdWithoutTypePrefix) {
    TemporaryDirectory directory;
    auto terrain = std::string{kGrass};
    const auto position = terrain.find("terrain.grassland");
    ASSERT_NE(position, std::string::npos);
    terrain.replace(position, std::string{"terrain.grassland"}.size(), "grassland");
    write_valid_ruleset(directory.path(), terrain);
    EXPECT_THROW(static_cast<void>(RulesetLoader::load(directory.path())), std::runtime_error);
}

TEST(RulesetLoader, RejectsMoveCostBelowOne) {
    TemporaryDirectory directory;
    auto terrain = std::string{kGrass};
    const auto position = terrain.find("move_cost=1");
    ASSERT_NE(position, std::string::npos);
    terrain.replace(position, std::string{"move_cost=1"}.size(), "move_cost=0");
    write_valid_ruleset(directory.path(), terrain);
    EXPECT_THROW(static_cast<void>(RulesetLoader::load(directory.path())), std::runtime_error);
}

TEST(RulesetLoader, RejectsIntegerNarrowingOutsideInt32) {
    TemporaryDirectory directory;
    auto terrain = std::string{kGrass};
    const auto position = terrain.find("food=1");
    ASSERT_NE(position, std::string::npos);
    terrain.replace(position, std::string{"food=1"}.size(), "food=2147483648");
    write_valid_ruleset(directory.path(), terrain);
    EXPECT_THROW(static_cast<void>(RulesetLoader::load(directory.path())), std::runtime_error);
}

TEST(RulesetLoader, RejectsUnresolvedDefinitionReference) {
    TemporaryDirectory directory;
    write_valid_ruleset(directory.path(), kGrass, "terrain.missing");
    EXPECT_THROW(static_cast<void>(RulesetLoader::load(directory.path())), std::runtime_error);
}

TEST(RulesetLoader, AllowsDisabledHistoryWithZeroCountAndBonus) {
    TemporaryDirectory directory;
    copy_default_ruleset(directory.path());
    auto civilization = read_text(directory.path() / "civilization.toml");
    replace_once(civilization, "ancient_site_count = 12", "ancient_site_count = 0");
    replace_once(civilization, "ancient_site_bonus = 10000", "ancient_site_bonus = 0");
    write_text(directory.path() / "civilization.toml", civilization);

    const auto ruleset = RulesetLoader::load(directory.path());
    EXPECT_EQ(ruleset.civilization_rules().history.ancient_site_count, 0U);
    EXPECT_EQ(ruleset.civilization_rules().history.ancient_city_count, 3U);
    EXPECT_EQ(ruleset.civilization_rules().history.ancient_town_count, 4U);
    EXPECT_EQ(ruleset.civilization_rules().history.ancient_site_bonus, 0);
}

TEST(RulesetLoader, RejectsDifferentCrossingKeysSharingResult) {
    TemporaryDirectory directory;
    copy_default_ruleset(directory.path());
    auto civilization = read_text(directory.path() / "civilization.toml");
    replace_once(civilization, "result = \"edge.bridge_wood_stream_road\"",
                 "result = \"edge.ford_stream_trail\"");
    write_text(directory.path() / "civilization.toml", civilization);

    try {
        static_cast<void>(RulesetLoader::load(directory.path()));
        FAIL() << "different crossing keys sharing one result should throw";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string{error.what()}.find("crossing result 重複"), std::string::npos);
    }
}

TEST(RulesetLoader, RejectsRepeatedHistoryRuinFeatureIds) {
    TemporaryDirectory directory;
    copy_default_ruleset(directory.path());
    auto civilization = read_text(directory.path() / "civilization.toml");
    replace_once(civilization,
                 "[\"feature.ruin_village\", \"feature.ruin_town\", \"feature.ruin_city\"]",
                 "[\"feature.ruin_village\", \"feature.ruin_town\", \"feature.ruin_town\"]");
    write_text(directory.path() / "civilization.toml", civilization);

    try {
        static_cast<void>(RulesetLoader::load(directory.path()));
        FAIL() << "repeated history ruin feature ids should throw";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string{error.what()}.find("ruin_features 三級 ID 不得重複"),
                  std::string::npos);
    }
}

TEST(RulesetLoader, RejectsHistoryRoadWithoutHigherMovementCost) {
    TemporaryDirectory directory;
    copy_default_ruleset(directory.path());
    auto edges = read_text(directory.path() / "edges.toml");
    replace_once(edges,
                 "id = \"edge.ancient_road\"\nname_key = \"edge.ancient_road.name\"\n"
                 "move_cost = 2",
                 "id = \"edge.ancient_road\"\nname_key = \"edge.ancient_road.name\"\n"
                 "move_cost = 1");
    write_text(directory.path() / "edges.toml", edges);

    EXPECT_THROW(static_cast<void>(RulesetLoader::load(directory.path())), std::runtime_error);
}

}  // namespace

#include "core/rules/ruleset.h"
#include "tests/rules/ruleset_test_support.h"

#include <cstdint>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::RulesetLoader;
using aetheria::tests::kGrass;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::write_text;
using aetheria::tests::write_valid_ruleset;

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

}  // namespace

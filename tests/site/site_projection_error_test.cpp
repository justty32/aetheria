#include "core/rules/ruleset.h"
#include "tests/worldgen/worldgen_test_support.h"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace {

TEST(SiteProjectionRules, MissingGroundDefinitionFailsDuringRulesetLoad) {
    aetheria::tests::TemporaryDirectory directory;
    aetheria::tests::copy_data_files(directory.path());
    const auto path = directory.path() / "site_projection.toml";
    std::ifstream input{path};
    ASSERT_TRUE(input.is_open());
    std::string text{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    constexpr std::string_view existing{"ground = \"ground.grass\""};
    const auto position = text.find(existing);
    ASSERT_NE(position, std::string::npos);
    text.replace(position, existing.size(), "ground = \"ground.missing\"");
    aetheria::tests::write_text(path, text);

    try {
        static_cast<void>(aetheria::rules::RulesetLoader::load(directory.path()));
        FAIL() << "missing GroundDef should fail during Ruleset load";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string{error.what()}.find("ground.missing"), std::string::npos);
    }
}

}  // namespace

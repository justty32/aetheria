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

TEST(SiteProjectionRules, InvalidBlockCutDistributionFailsDuringRulesetLoad) {
    aetheria::tests::TemporaryDirectory directory;
    aetheria::tests::copy_data_files(directory.path());
    const auto path = directory.path() / "site_projection.toml";
    std::ifstream input{path};
    ASSERT_TRUE(input.is_open());
    std::string text{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    constexpr std::string_view existing{"block_cut_min_percent = 36"};
    const auto position = text.find(existing);
    ASSERT_NE(position, std::string::npos);
    text.replace(position, existing.size(), "block_cut_min_percent = 50");
    aetheria::tests::write_text(path, text);

    EXPECT_THROW(static_cast<void>(aetheria::rules::RulesetLoader::load(directory.path())),
                 std::runtime_error);
}

TEST(SiteProjectionRules, EnabledZoneWithoutBuildingDefinitionFailsDuringRulesetLoad) {
    aetheria::tests::TemporaryDirectory directory;
    aetheria::tests::copy_data_files(directory.path());
    const auto path = directory.path() / "site_city.toml";
    std::ifstream input{path};
    ASSERT_TRUE(input.is_open());
    std::string text{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    constexpr std::string_view marker{"zone = \"commercial\""};
    auto position = text.find(marker);
    ASSERT_NE(position, std::string::npos);
    for (std::size_t building = 0; building < 2; ++building) {
        position = text.find(marker, position + 1U);
        ASSERT_NE(position, std::string::npos);
        text.replace(position, marker.size(), "zone = \"residential\"");
    }
    aetheria::tests::write_text(path, text);

    try {
        static_cast<void>(aetheria::rules::RulesetLoader::load(directory.path()));
        FAIL() << "enabled zone without building def should fail during Ruleset load";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string{error.what()}.find("建築 def"), std::string::npos);
    }
}

TEST(SiteProjectionRules, EnabledZoneWithoutQuotaDefinitionFailsDuringRulesetLoad) {
    aetheria::tests::TemporaryDirectory directory;
    aetheria::tests::copy_data_files(directory.path());
    const auto path = directory.path() / "site_city.toml";
    std::ifstream input{path};
    ASSERT_TRUE(input.is_open());
    std::string text{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    constexpr std::string_view residential_quota{R"([[quotas]]
zone = "residential"
driver = "population"
units_per_block = 250
max_percent = 70

)"};
    const auto position = text.find(residential_quota);
    ASSERT_NE(position, std::string::npos);
    text.erase(position, residential_quota.size());
    aetheria::tests::write_text(path, text);

    try {
        static_cast<void>(aetheria::rules::RulesetLoader::load(directory.path()));
        FAIL() << "enabled zone without quota def should fail during Ruleset load";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string{error.what()}.find("配額 def"), std::string::npos);
    }
}

TEST(SiteProjectionRules, FactionStyleRejectsNonLandmarkReference) {
    aetheria::tests::TemporaryDirectory directory;
    aetheria::tests::copy_data_files(directory.path());
    const auto path = directory.path() / "site_city.toml";
    std::ifstream input{path};
    ASSERT_TRUE(input.is_open());
    std::string text{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    constexpr std::string_view existing{
        "landmarks = [\"building.palace\", \"building.grand_temple\"]"};
    const auto position = text.find(existing);
    ASSERT_NE(position, std::string::npos);
    text.replace(position, existing.size(), "landmarks = [\"building.cottage\"]");
    aetheria::tests::write_text(path, text);

    EXPECT_THROW(static_cast<void>(aetheria::rules::RulesetLoader::load(directory.path())),
                 std::runtime_error);
}

TEST(SiteProjectionRules, GateDefinitionMustBeWallGateAndOpenable) {
    aetheria::tests::TemporaryDirectory directory;
    aetheria::tests::copy_data_files(directory.path());
    const auto path = directory.path() / "site_city.toml";
    std::ifstream input{path};
    ASSERT_TRUE(input.is_open());
    std::string text{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    constexpr std::string_view existing{"gate_edge = \"edge.city_gate\""};
    const auto position = text.find(existing);
    ASSERT_NE(position, std::string::npos);
    text.replace(position, existing.size(), "gate_edge = \"edge.road\"");
    aetheria::tests::write_text(path, text);

    EXPECT_THROW(static_cast<void>(aetheria::rules::RulesetLoader::load(directory.path())),
                 std::runtime_error);
}

}  // namespace

#include "core/rules/ruleset.h"
#include "tests/rules/ruleset_test_support.h"
#include "tests/support/ruleset_fixture.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::RulesetLoader;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::tests::write_text;

void copy_default_ruleset(const std::filesystem::path& destination) {
    std::filesystem::copy(
        AETHERIA_SOURCE_DIR "/data", destination,
        std::filesystem::copy_options::recursive |
            std::filesystem::copy_options::overwrite_existing);
}

[[nodiscard]] std::string read_text(const std::filesystem::path& path) {
    std::ifstream stream{path};
    if (!stream) {
        throw std::runtime_error{"Cannot read diplomacy rules test file"};
    }
    return {std::istreambuf_iterator<char>{stream},
            std::istreambuf_iterator<char>{}};
}

TEST(DiplomacyRules,
     LoadsFourDistinctRatesTreatiesCasusBelliAndFormulaParameters) {
    const auto& rules = test_ruleset().diplomacy_rules();
    ASSERT_TRUE(rules.loaded);
    EXPECT_GT(rules.reversion.favor, rules.reversion.fear);
    EXPECT_GT(rules.reversion.fear, rules.reversion.trust);
    EXPECT_GT(rules.reversion.trust, rules.reversion.grievance);
    EXPECT_EQ(rules.treaties.size(), 7U);
    EXPECT_EQ(rules.casus_belli.size(), 5U);
    EXPECT_EQ(test_ruleset()
                  .treaty(*test_ruleset().find_treaty("treaty.marriage"))
                  ->duration_xun,
              0U);
    EXPECT_EQ(test_ruleset()
                  .casus_belli(
                      *test_ruleset().find_casus_belli("casus_belli.revenge"))
                  ->duration_xun,
              36U);
}

TEST(DiplomacyRules, AddingTreatyRequiresOnlyOneDataEntry) {
    TemporaryDirectory directory;
    copy_default_ruleset(directory.path());
    auto diplomacy = read_text(directory.path() / "diplomacy.toml");
    diplomacy += R"(

[[treaties]]
id = "treaty.hostage_exchange"
duration_xun = 12
condition = "hostages_available"
breach_favor = -300
breach_trust = -700
breach_grievance = 200
renewable = true
)";
    write_text(directory.path() / "diplomacy.toml", diplomacy);

    const auto ruleset = RulesetLoader::load(directory.path());
    ASSERT_EQ(ruleset.diplomacy_rules().treaties.size(), 8U);
    const auto added = ruleset.find_treaty("treaty.hostage_exchange");
    ASSERT_TRUE(added.has_value());
    const auto* definition = ruleset.treaty(*added);
    ASSERT_NE(definition, nullptr);
    EXPECT_EQ(definition->duration_xun, 12U);
    EXPECT_EQ(definition->condition, "hostages_available");
    EXPECT_TRUE(definition->renewable);
}

TEST(DiplomacyRules, RejectsEqualizedReversionRates) {
    TemporaryDirectory directory;
    copy_default_ruleset(directory.path());
    auto diplomacy = read_text(directory.path() / "diplomacy.toml");
    const auto position = diplomacy.find("fear_reversion = 500");
    ASSERT_NE(position, std::string::npos);
    diplomacy.replace(position, std::string{"fear_reversion = 500"}.size(),
                      "fear_reversion = 1000");
    write_text(directory.path() / "diplomacy.toml", diplomacy);

    EXPECT_THROW(static_cast<void>(RulesetLoader::load(directory.path())),
                 std::runtime_error);
}

} // namespace

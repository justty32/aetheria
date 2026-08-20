#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/worldgen/worldgen_test_support.h"

#include <fstream>
#include <iterator>
#include <stdexcept>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::Ruleset;
using aetheria::rules::RulesetLoader;
using aetheria::tests::copy_data_files;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::tests::write_text;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::hash_stage;
using aetheria::worldgen::RegionGenerationConfig;
using aetheria::worldgen::RegionSlowVariables;

[[nodiscard]] Ruleset ruleset_with_governance_cost(std::int64_t cost) {
    TemporaryDirectory directory;
    copy_data_files(directory.path());
    const auto path = directory.path() / "civilization.toml";
    std::ifstream input{path};
    if (!input.is_open()) {
        throw std::runtime_error{"faction fixture open failed"};
    }
    std::string text{std::istreambuf_iterator<char>{input},
                     std::istreambuf_iterator<char>{}};
    const auto position = text.find("governance_max_cost = 256");
    if (position == std::string::npos) {
        throw std::runtime_error{"faction fixture field missing"};
    }
    text.replace(position, std::string_view{"governance_max_cost = 256"}.size(),
                 "governance_max_cost = " + std::to_string(cost));
    write_text(path, text);
    return RulesetLoader::load(directory.path());
}

TEST(FactionGenerationStage, FactionDataAndConfigAreIsolatedToStagesTwelveAndLater) {
    constexpr RegionSlowVariables slow{0, 128, 96};
    constexpr auto seed = UINT64_C(12345);
    const auto before = build_skeleton(slow, seed, test_ruleset());
    const auto changed_rules = ruleset_with_governance_cost(20);
    const auto after = build_skeleton(slow, seed, changed_rules);
    EXPECT_EQ(hash_stage(before.roads), hash_stage(after.roads));
    EXPECT_EQ(hash_stage(before.portals), hash_stage(after.portals));
    EXPECT_NE(hash_stage(before.factions), hash_stage(after.factions));

    RegionGenerationConfig original;
    auto portal_config = original;
    auto faction_config = original;
    portal_config.portals.road_tier = 1;
    faction_config.factions.first_faction_id = 7;
    const auto base_groups = aetheria::worldgen::generation_parameter_hashes(original);
    const auto portal_groups = aetheria::worldgen::generation_parameter_hashes(portal_config);
    const auto faction_groups = aetheria::worldgen::generation_parameter_hashes(faction_config);
    for (std::size_t index = 0; index < base_groups.groups.size(); ++index) {
        EXPECT_EQ(base_groups.groups[index] != portal_groups.groups[index], index == 10U);
        EXPECT_EQ(base_groups.groups[index] != faction_groups.groups[index], index == 11U);
    }
}

}  // namespace

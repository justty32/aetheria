#include "core/rules/ruleset.h"
#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/worldgen/worldgen_test_support.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

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
using aetheria::worldgen::RegionBuildResult;
using aetheria::worldgen::RegionGenerationConfig;
using aetheria::worldgen::RegionSlowVariables;

[[nodiscard]] Ruleset ruleset_replacing(std::string_view before, std::string_view after) {
    TemporaryDirectory directory;
    copy_data_files(directory.path());
    const auto path = directory.path() / "civilization.toml";
    std::ifstream input{path};
    if (!input.is_open()) {
        throw std::runtime_error{"history isolation fixture could not open civilization.toml"};
    }
    std::string text{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    const auto position = text.find(before);
    if (position == std::string::npos) {
        throw std::runtime_error{"history isolation fixture replacement failed"};
    }
    text.replace(position, before.size(), after);
    write_text(path, text);
    return RulesetLoader::load(directory.path());
}

[[nodiscard]] std::array<std::uint64_t, 10> stage_hashes(const RegionBuildResult& result) {
    return {hash_stage(result.plates),   hash_stage(result.height),
            hash_stage(result.erosion), hash_stage(result.climate),
            hash_stage(result.rivers),  hash_stage(result.biome),
            hash_stage(result.features), hash_stage(result.history),
            hash_stage(result.cities),  hash_stage(result.roads)};
}

void print_hashes(std::string_view label, const std::array<std::uint64_t, 10>& hashes) {
    std::cout << label;
    for (const auto hash : hashes) {
        std::cout << ' ' << hash;
    }
    std::cout << '\n';
}

TEST(HistoryGenerationStage, DisablingAncientSitesChangesOnlyStagesEightThroughTen) {
    constexpr auto seed = UINT64_C(20260820);
    const RegionSlowVariables slow{19, 128, 96};
    const auto enabled = build_skeleton(slow, seed, test_ruleset());
    const auto disabled_rules =
        ruleset_replacing("ancient_site_count = 12", "ancient_site_count = 0");
    const auto disabled = build_skeleton(slow, seed, disabled_rules);
    const auto enabled_hashes = stage_hashes(enabled);
    const auto disabled_hashes = stage_hashes(disabled);

    print_hashes("history_count12_hashes", enabled_hashes);
    print_hashes("history_count0_hashes", disabled_hashes);
    for (std::size_t index = 0; index < 7U; ++index) {
        EXPECT_EQ(enabled_hashes[index], disabled_hashes[index]);
    }
    EXPECT_NE(enabled_hashes[7], disabled_hashes[7]);
    EXPECT_NE(enabled_hashes[8], disabled_hashes[8]);
    EXPECT_NE(enabled_hashes[9], disabled_hashes[9]);
}

TEST(HistoryGenerationStage, HistoryConfigOwnsOnlyTheEighthParameterGroup) {
    RegionGenerationConfig original;
    auto changed = original;
    changed.history.minimum_score_bias = std::numeric_limits<std::int16_t>::max();
    const auto before_groups = aetheria::worldgen::generation_parameter_hashes(original);
    const auto after_groups = aetheria::worldgen::generation_parameter_hashes(changed);
    constexpr std::array<std::string_view, 12> names{
        "plates",   "height", "erosion", "climate", "rivers",  "biome",
        "features", "history", "cities", "roads",   "portals", "factions"};
    ASSERT_EQ(before_groups.groups.size(), 12U);
    for (std::size_t index = 0; index < before_groups.groups.size(); ++index) {
        EXPECT_EQ(aetheria::worldgen::generation_parameter_group_name(index), names[index]);
        if (index == 7U) {
            EXPECT_NE(before_groups.groups[index], after_groups.groups[index]);
        } else {
            EXPECT_EQ(before_groups.groups[index], after_groups.groups[index]);
        }
    }

    const RegionSlowVariables slow{29, 128, 96};
    const auto before = build_skeleton(slow, UINT64_C(292929), test_ruleset(), original);
    const auto after = build_skeleton(slow, UINT64_C(292929), test_ruleset(), changed);
    const auto before_hashes = stage_hashes(before);
    const auto after_hashes = stage_hashes(after);
    print_hashes("history_config_default_hashes", before_hashes);
    print_hashes("history_config_changed_hashes", after_hashes);
    for (std::size_t index = 0; index < 7U; ++index) {
        EXPECT_EQ(before_hashes[index], after_hashes[index]);
    }
    EXPECT_NE(before_hashes[7], after_hashes[7]);
}

}  // namespace

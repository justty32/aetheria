#include "core/worldgen/gen_stage_ids.h"
#include "core/worldgen/region_generator.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/worldgen/worldgen_test_support.h"

#include <algorithm>
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
using aetheria::worldgen::generate_roads;
using aetheria::worldgen::generate_cities;
using aetheria::worldgen::hash_stage;
using aetheria::worldgen::RegionBuildResult;
using aetheria::worldgen::RegionSlowVariables;

[[nodiscard]] Ruleset ruleset_replacing(std::string_view before, std::string_view after) {
    TemporaryDirectory directory;
    copy_data_files(directory.path());
    const auto path = directory.path() / "civilization.toml";
    std::ifstream input{path};
    if (!input.is_open()) {
        throw std::runtime_error{"history test fixture could not open civilization.toml"};
    }
    std::string text{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    const auto position = text.find(before);
    if (position == std::string::npos) {
        throw std::runtime_error{"history test fixture replacement failed"};
    }
    text.replace(position, before.size(), after);
    write_text(path, text);
    return RulesetLoader::load(directory.path());
}

[[nodiscard]] std::size_t modern_overlap(const aetheria::worldgen::HistoryStageOutput& history,
                                         const aetheria::worldgen::CityStageOutput& cities) {
    return static_cast<std::size_t>(std::ranges::count_if(cities.cities, [&](const auto& city) {
        return history.survivor[city.canonical_id] != 0;
    }));
}

struct RoadReuse {
    std::size_t reused{};
    std::size_t eligible{};
    std::size_t skipped_river{};
};

[[nodiscard]] RoadReuse measure_reuse(const RegionBuildResult& result) {
    RoadReuse measured;
    const auto width = result.history.features.width;
    const auto height = result.history.features.height;
    const auto ancient_road = test_ruleset().civilization_rules().history.road_edge;
    auto inspect = [&](std::size_t offset) {
        if (result.history.skipped_river_edges[offset] != 0) {
            ++measured.skipped_river;
            return;
        }
        if (result.roads.usage[offset] == 0) {
            return;
        }
        ++measured.eligible;
        if (result.history.edges[offset] == ancient_road) {
            ++measured.reused;
        }
    };
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            const auto tile = y * width + x;
            if (x + 1U < width) {
                inspect(tile * 4U + 1U);
            }
            if (y + 1U < height) {
                inspect(tile * 4U + 2U);
            }
        }
    }
    return measured;
}

TEST(HistoryGenerationStage, BonusRuinsSurvivalBoundaryAndRoadReuseAreObservable) {
    constexpr auto seed = UINT64_C(20260820);
    const RegionSlowVariables slow{7, 128, 96};
    const auto enabled = build_skeleton(slow, seed, test_ruleset());
    const auto zero_bonus_rules =
        ruleset_replacing("ancient_site_bonus = 10000", "ancient_site_bonus = 0");
    std::size_t enabled_overlap{};
    std::size_t zero_overlap{};
    for (std::uint32_t region_id = 0; region_id < 8U; ++region_id) {
        const RegionSlowVariables sample{region_id, 128, 96};
        const auto sample_enabled = build_skeleton(sample, seed, test_ruleset());
        const auto sample_zero = generate_cities(
            sample_enabled.skeleton.elevation, sample_enabled.climate, sample_enabled.rivers,
            sample_enabled.biome, sample_enabled.history, zero_bonus_rules,
            aetheria::worldgen::derive_region_stage_seed(
                seed, region_id, aetheria::worldgen::detail::kCityStageId),
            {});
        enabled_overlap += modern_overlap(sample_enabled.history, sample_enabled.cities);
        zero_overlap += modern_overlap(sample_enabled.history, sample_zero);
    }
    EXPECT_GT(enabled_overlap, zero_overlap);

    const auto& history_rules = test_ruleset().civilization_rules().history;
    std::array<std::size_t, 3> ruins{};
    for (const auto& site : enabled.history.ancient_sites.cities) {
        if (enabled.history.survivor[site.canonical_id] == 0) {
            const auto tier = static_cast<std::size_t>(site.tier) - 1U;
            ASSERT_LT(tier, ruins.size());
            EXPECT_EQ(enabled.history.features.feature[site.canonical_id],
                      history_rules.ruin_features[tier]);
            ++ruins[tier];
        }
    }
    const auto survivors = static_cast<std::size_t>(
        std::ranges::count(enabled.history.survivor, std::uint8_t{1}));
    EXPECT_EQ(survivors, enabled.history.ancient_sites.cities.size() *
                             history_rules.survivor_percent / 100U);
    EXPECT_EQ(ruins[0] + ruins[1] + ruins[2],
              enabled.history.ancient_sites.cities.size() - survivors);
    ASSERT_GT(survivors, 0U);
    ASSERT_GT(ruins[0] + ruins[1] + ruins[2], 0U);

    auto minimum_survivor = std::numeric_limits<std::int32_t>::max();
    auto maximum_ruined = std::numeric_limits<std::int32_t>::min();
    for (const auto& site : enabled.history.ancient_sites.cities) {
        if (enabled.history.survivor[site.canonical_id] != 0) {
            minimum_survivor = std::min(minimum_survivor, site.score);
        } else {
            maximum_ruined = std::max(maximum_ruined, site.score);
        }
    }
    EXPECT_GE(minimum_survivor, maximum_ruined);

    const auto reuse = measure_reuse(enabled);
    ASSERT_GT(reuse.eligible, 0U);
    EXPECT_GT(reuse.reused, 0U);
    auto without_ancient_roads = enabled.history;
    for (auto& edge : without_ancient_roads.edges) {
        if (edge == history_rules.road_edge) {
            edge = enabled.skeleton.definitions.no_edge;
        }
    }
    const auto control_roads = generate_roads(
        enabled.skeleton.elevation, enabled.climate, enabled.rivers, enabled.biome,
        without_ancient_roads, enabled.cities, enabled.skeleton.definitions, test_ruleset(),
        aetheria::worldgen::derive_region_stage_seed(
            seed, slow.region_id, aetheria::worldgen::detail::kRoadStageId),
        {});
    EXPECT_NE(hash_stage(enabled.roads), hash_stage(control_roads));

    std::cout << "history_feedback enabled_overlap=" << enabled_overlap
              << " zero_bonus_overlap=" << zero_overlap << " survivors=" << survivors
              << " ruins_village=" << ruins[0] << " ruins_town=" << ruins[1]
              << " ruins_city=" << ruins[2]
              << " min_survivor_score=" << minimum_survivor
              << " max_ruined_score=" << maximum_ruined << '\n'
              << "ancient_road_reuse reused=" << reuse.reused
              << " eligible_modern_edges=" << reuse.eligible
              << " percent=" << (100.0 * static_cast<double>(reuse.reused) /
                                   static_cast<double>(reuse.eligible))
              << " skipped_river_edges=" << reuse.skipped_river
              << " stage10_with_ancient_hash=" << hash_stage(enabled.roads)
              << " stage10_without_ancient_hash=" << hash_stage(control_roads) << '\n';
}

}  // namespace

#pragma once

// 上古身分測試專用的規則覆寫與跨 Region 量測 helper。

#include "core/worldgen/region_generator.h"
#include "tests/worldgen/worldgen_test_support.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace aetheria::tests::history_identity {

constexpr std::string_view kIdentityBlock = R"([history]
freshwater_weight = 500
farmland_weight = 6
harbor_weight = 60
defense_weight = 120
resource_weight = 300
bottleneck_weight = 700
extreme_climate_penalty = -260
high_elevation_penalty = -180
ancient_site_count = 12
ancient_city_count = 3
ancient_town_count = 4
minimum_spacing = [5, 8, 12])";

constexpr std::string_view kSharedWeightBlock = R"([history]
freshwater_weight = 500
farmland_weight = 18
harbor_weight = 220
defense_weight = 24
resource_weight = 160
bottleneck_weight = 420
extreme_climate_penalty = -260
high_elevation_penalty = -180
ancient_site_count = 12
ancient_city_count = 3
ancient_town_count = 4
minimum_spacing = [5, 8, 12])";

[[nodiscard]] inline rules::Ruleset ruleset_replacing(std::string_view before,
                                                       std::string_view after) {
    TemporaryDirectory directory;
    copy_data_files(directory.path());
    const auto path = directory.path() / "civilization.toml";
    std::ifstream input{path};
    if (!input.is_open()) {
        throw std::runtime_error{"history identity fixture could not open civilization.toml"};
    }
    std::string text{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    const auto position = text.find(before);
    if (position == std::string::npos) {
        throw std::runtime_error{"history identity fixture replacement failed"};
    }
    text.replace(position, before.size(), after);
    write_text(path, text);
    return rules::RulesetLoader::load(directory.path());
}

struct IdentityMetrics {
    std::size_t ancient_sites{};
    std::size_t exact_site_overlap{};
    std::size_t survivors{};
    std::size_t survivor_overlap{};

    IdentityMetrics& operator+=(const IdentityMetrics& next) {
        ancient_sites += next.ancient_sites;
        exact_site_overlap += next.exact_site_overlap;
        survivors += next.survivors;
        survivor_overlap += next.survivor_overlap;
        return *this;
    }
};

[[nodiscard]] inline IdentityMetrics measure_identity(const worldgen::RegionBuildResult& result) {
    IdentityMetrics metrics;
    metrics.ancient_sites = result.history.ancient_sites.cities.size();
    for (const auto& ancient : result.history.ancient_sites.cities) {
        metrics.exact_site_overlap += static_cast<std::size_t>(std::ranges::any_of(
            result.cities.cities,
            [&](const auto& modern) { return modern.canonical_id == ancient.canonical_id; }));
        if (result.history.survivor[ancient.canonical_id] != 0) {
            ++metrics.survivors;
        }
    }
    for (const auto& modern : result.cities.cities) {
        metrics.survivor_overlap += result.history.survivor[modern.canonical_id] != 0;
    }
    return metrics;
}

struct RoadMetrics {
    std::size_t ancient_edges{};
    std::size_t unoverwritten_edges{};
    std::size_t reused{};
    std::size_t eligible{};

    RoadMetrics& operator+=(const RoadMetrics& next) {
        ancient_edges += next.ancient_edges;
        unoverwritten_edges += next.unoverwritten_edges;
        reused += next.reused;
        eligible += next.eligible;
        return *this;
    }
};

[[nodiscard]] inline RoadMetrics measure_roads(const worldgen::RegionBuildResult& result,
                                               const rules::Ruleset& ruleset) {
    RoadMetrics metrics;
    const auto ancient_road = ruleset.civilization_rules().history.road_edge;
    const auto width = result.history.features.width;
    const auto height = result.history.features.height;
    auto inspect = [&](std::size_t offset) {
        if (result.history.edges[offset] == ancient_road) {
            ++metrics.ancient_edges;
            metrics.unoverwritten_edges += result.roads.edges[offset] == ancient_road;
        }
        if (result.history.skipped_river_edges[offset] == 0 && result.roads.usage[offset] != 0) {
            ++metrics.eligible;
            metrics.reused += result.history.edges[offset] == ancient_road;
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
    return metrics;
}

[[nodiscard]] inline double reuse_percent(const RoadMetrics& metrics) {
    return 100.0 * static_cast<double>(metrics.reused) /
           static_cast<double>(metrics.eligible);
}

}  // namespace aetheria::tests::history_identity

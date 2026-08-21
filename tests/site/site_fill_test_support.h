#pragma once

// M3.1 測試共用的 Site 骨架、填充計數與臨街驗證 helper。

#include "core/site/site_projection.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
#include <array>
#include <cstdint>

#include <gtest/gtest.h>

namespace aetheria::tests {

struct FillCounts {
    std::size_t residential_blocks{};
    std::size_t commercial_blocks{};
    std::size_t residential_buildings{};
    std::size_t commercial_buildings{};
};

[[nodiscard]] inline site::SiteSlowVars fill_slow_vars(bool four_roads = false) {
    const auto& rules = test_ruleset();
    const auto none = *rules.find_edge("edge.none");
    const auto road = *rules.find_edge("edge.road");
    return {*rules.find_terrain("terrain.grassland"),
            *rules.find_relief("relief.plain"),
            *rules.find_feature("feature.none"),
            3200,
            four_roads ? std::array{road, road, road, road}
                       : std::array{road, none, none, road}};
}

[[nodiscard]] inline FillCounts fill_counts(const site::SiteProceduralLayer& layer) {
    FillCounts result;
    result.residential_blocks =
        std::ranges::count(layer.block_zoning, site::SiteZoning::Residential);
    result.commercial_blocks =
        std::ranges::count(layer.block_zoning, site::SiteZoning::Commercial);
    for (const auto& building : layer.buildings) {
        const auto* def = test_ruleset().building(building.def);
        EXPECT_NE(def, nullptr);
        if (def != nullptr && def->zone == rules::SiteFillZone::Residential) {
            ++result.residential_buildings;
        } else if (def != nullptr) {
            ++result.commercial_buildings;
        }
    }
    return result;
}

[[nodiscard]] inline site::SiteProceduralLayer generate_fill(const site::SiteFastVars& fast) {
    constexpr auto seed = UINT64_C(0xF1131);
    return site::populate(site::build_site_skeleton(fill_slow_vars(), seed, test_ruleset()), fast,
                          test_ruleset());
}

[[nodiscard]] inline site::SiteProceduralLayer generate_fill(const site::SiteFastVars& fast,
                                                             const site::SiteSlowVars& slow) {
    constexpr auto seed = UINT64_C(0xF1131);
    return site::populate(site::build_site_skeleton(slow, seed, test_ruleset()), fast,
                          test_ruleset());
}

[[nodiscard]] inline std::size_t fill_tile_index(std::uint16_t x, std::uint16_t y) {
    return static_cast<std::size_t>(y) * site::kSiteWidth + x;
}

[[nodiscard]] inline bool frontage_is_road(const site::SiteProceduralLayer& layer,
                                           const site::ProceduralBuilding& building) {
    const auto x_end = static_cast<std::uint16_t>(building.origin.x + building.width);
    const auto y_end = static_cast<std::uint16_t>(building.origin.y + building.height);
    if (building.frontage == site::SiteBoundarySide::North) {
        if (building.origin.y == 0) {
            return false;
        }
        for (std::uint16_t x = building.origin.x; x < x_end; ++x) {
            if (layer.skeleton.roads[fill_tile_index(x, building.origin.y - 1U)] == 0) {
                return false;
            }
        }
        return true;
    }
    if (building.frontage == site::SiteBoundarySide::South) {
        for (std::uint16_t x = building.origin.x; x < x_end; ++x) {
            if (y_end >= site::kSiteHeight ||
                layer.skeleton.roads[fill_tile_index(x, y_end)] == 0) {
                return false;
            }
        }
        return true;
    }
    for (std::uint16_t y = building.origin.y; y < y_end; ++y) {
        const auto road_x = building.frontage == site::SiteBoundarySide::West
                                ? static_cast<int>(building.origin.x) - 1
                                : static_cast<int>(x_end);
        if (road_x < 0 || road_x >= static_cast<int>(site::kSiteWidth) ||
            layer.skeleton.roads[fill_tile_index(static_cast<std::uint16_t>(road_x), y)] == 0) {
            return false;
        }
    }
    return true;
}

}  // namespace aetheria::tests

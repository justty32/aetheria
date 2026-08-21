#include "core/site/site_wilderness_detail.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace aetheria::site::wilderness_detail {
namespace {

[[nodiscard]] std::vector<SiteXY> crossing_tiles(const WildernessSlowVars& slow,
                                                 const rules::Ruleset& ruleset,
                                                 std::uint32_t required_flag) {
    std::vector<SiteXY> result;
    for (std::size_t side = 0; side < slow.boundaries.size(); ++side) {
        for (const auto& crossing : slow.boundaries[side].crossings) {
            const auto* definition = ruleset.edge(crossing.kind);
            if (definition != nullptr && (definition->flags & required_flag) != 0) {
                result.push_back(boundary_tile(static_cast<SiteBoundarySide>(side), crossing.pos));
            }
        }
    }
    return result;
}

void carve_water(WildernessSkeleton& result, const std::vector<SiteXY>& path,
                 rules::GroundId water_ground) {
    for (const auto tile : path) {
        const auto index = tile_index(tile);
        result.terrain.water[index] = std::max<std::uint8_t>(result.terrain.water[index], 1U);
        result.terrain.ground[index] = water_ground;
        result.terrain.buildable[index] = 0;
    }
}

void carve_lake(WildernessSkeleton& result, SiteXY center, rules::GroundId water_ground) {
    for (std::int32_t dy = -2; dy <= 2; ++dy) {
        for (std::int32_t dx = -2; dx <= 2; ++dx) {
            const auto x = static_cast<std::int32_t>(center.x) + dx;
            const auto y = static_cast<std::int32_t>(center.y) + dy;
            if (x < 1 || y < 1 || x >= static_cast<std::int32_t>(kSiteWidth - 1U) ||
                y >= static_cast<std::int32_t>(kSiteHeight - 1U) || dx * dx + dy * dy > 5) {
                continue;
            }
            const SiteXY tile{static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y)};
            const auto index = tile_index(tile);
            result.terrain.water[index] = 2;
            result.terrain.ground[index] = water_ground;
            result.terrain.buildable[index] = 0;
        }
    }
}

void mark_road(WildernessSkeleton& result, const std::vector<SiteXY>& path) {
    for (const auto tile : path) {
        const auto index = tile_index(tile);
        if (result.terrain.water[index] != 0 && result.terrain.roads[index] == 0) {
            ++result.bridge_count;
        }
        result.terrain.roads[index] = 1;
        result.terrain.buildable[index] = 1;
    }
}

}  // namespace

void generate_wilderness_paths(WildernessSkeleton& result, const WildernessSlowVars& slow,
                               const rules::Ruleset& ruleset) {
    const auto rivers = crossing_tiles(slow, ruleset, rules::kEdgeRiverFlag);
    const auto roads = crossing_tiles(slow, ruleset, rules::kEdgeRoadFlag);
    const auto water_ground = ruleset.find_ground("ground.water");
    if (!water_ground.has_value()) {
        throw std::runtime_error{"荒野 W2 缺少 ground.water"};
    }

    if (rivers.size() == 1U) {
        const SiteXY lake{static_cast<std::uint16_t>(kSiteWidth / 2U),
                          static_cast<std::uint16_t>(kSiteHeight / 2U)};
        carve_water(result, find_path(result.terrain, rivers.front(), lake, false), *water_ground);
        carve_lake(result, lake, *water_ground);
        result.river_path_count = 1;
        result.lake_count = 1;
    } else if (rivers.size() > 1U) {
        for (std::size_t index = 1; index < rivers.size(); ++index) {
            carve_water(result, find_path(result.terrain, rivers.front(), rivers[index], false),
                        *water_ground);
            ++result.river_path_count;
        }
    }

    if (roads.size() == 1U) {
        mark_road(result, find_path(result.terrain, roads.front(), result.terrain.city_center, true));
        result.road_path_count = 1;
    } else if (roads.size() > 1U) {
        for (std::size_t index = 1; index < roads.size(); ++index) {
            mark_road(result, find_path(result.terrain, roads.front(), roads[index], true));
            ++result.road_path_count;
        }
    }
}

}  // namespace aetheria::site::wilderness_detail

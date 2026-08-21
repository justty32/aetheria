#pragma once

#include "core/site/site_wilderness.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace aetheria::tests {

inline constexpr std::uint64_t kWildWorldSeed = UINT64_C(0x73A9C24E18B5D60F);
inline constexpr std::uint32_t kWildRegionId = 19;
inline constexpr world::RegionXY kWildCenter{2, 2};

[[nodiscard]] inline world::RegionTiles wilderness_region() {
    const auto& ruleset = test_ruleset();
    world::RegionTiles tiles{5, 5};
    std::ranges::fill(tiles.base, *ruleset.find_terrain("terrain.grassland"));
    std::ranges::fill(tiles.relief, *ruleset.find_relief("relief.plain"));
    std::ranges::fill(tiles.feature, *ruleset.find_feature("feature.none"));
    std::ranges::fill(tiles.edges, *ruleset.find_edge("edge.none"));
    for (std::uint32_t y = 0; y < tiles.height; ++y) {
        for (std::uint32_t x = 0; x < tiles.width; ++x) {
            const auto coordinate = world::RegionXY{static_cast<std::int16_t>(x),
                                                    static_cast<std::int16_t>(y)};
            tiles.elevation[tiles.index_of(coordinate)] =
                static_cast<std::uint16_t>(1200U + y * 180U + x * 20U);
        }
    }
    return tiles;
}

inline void add_crossings(world::RegionTiles& tiles) {
    const auto& ruleset = test_ruleset();
    tiles.set_edge(kWildCenter, {2, 1}, *ruleset.find_edge("edge.river"));
    tiles.set_edge(kWildCenter, {2, 3}, *ruleset.find_edge("edge.stream"));
    tiles.set_edge(kWildCenter, {1, 2}, *ruleset.find_edge("edge.trail"));
    tiles.set_edge(kWildCenter, {3, 2}, *ruleset.find_edge("edge.highway"));
}

[[nodiscard]] inline std::size_t nonzero_count(const std::vector<std::uint8_t>& values) {
    return static_cast<std::size_t>(
        std::ranges::count_if(values, [](std::uint8_t value) { return value != 0; }));
}

[[nodiscard]] inline site::BoundaryProfile actual_boundary(
    const site::SiteSkeleton& terrain, site::SiteBoundarySide side,
    std::vector<site::BoundaryCrossing> crossings = {}) {
    site::BoundaryProfile result;
    result.crossings = std::move(crossings);
    for (std::uint8_t position = 0; position < site::kSiteWidth; ++position) {
        site::SiteXY tile;
        switch (side) {
        case site::SiteBoundarySide::North:
            tile = {position, 0};
            break;
        case site::SiteBoundarySide::East:
            tile = {site::kSiteWidth - 1U, position};
            break;
        case site::SiteBoundarySide::South:
            tile = {position, site::kSiteHeight - 1U};
            break;
        case site::SiteBoundarySide::West:
            tile = {0, position};
            break;
        }
        const auto index = static_cast<std::size_t>(tile.y) * site::kSiteWidth + tile.x;
        result.elevation[position] = terrain.elevation[index];
        result.ground[position] = terrain.ground[index];
        result.water_depth[position] = terrain.water[index];
        result.edges[position] =
            terrain.edges[index * 4U + static_cast<std::size_t>(side)];
    }
    return result;
}

}  // namespace aetheria::tests

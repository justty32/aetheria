#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "core/local/local_tiles.h"
#include "tests/support/ruleset_fixture.h"

namespace aetheria::tests {

inline constexpr std::uint64_t kLocalSiteSeed = UINT64_C(0xA4217B3E9C60D5F8);
inline constexpr site::SiteXY kLocalCenter{32, 32};

[[nodiscard]] inline site::SiteProceduralLayer open_site_layer() {
    const auto& ruleset = test_ruleset();
    const auto grass = *ruleset.find_ground("ground.grass");
    const auto none = *ruleset.find_edge("edge.none");
    site::SiteSkeleton skeleton;
    skeleton.ground.assign(site::kSiteTileCount, grass);
    skeleton.edges.assign(site::kSiteTileCount * 4U, none);
    skeleton.elevation.resize(site::kSiteTileCount);
    skeleton.water.assign(site::kSiteTileCount, 0);
    skeleton.roads.assign(site::kSiteTileCount, 0);
    skeleton.buildable.assign(site::kSiteTileCount, 1);
    skeleton.city_center = {site::kSiteWidth / 2U, site::kSiteHeight / 2U};
    for (std::uint16_t y = 0; y < site::kSiteHeight; ++y) {
        for (std::uint16_t x = 0; x < site::kSiteWidth; ++x) {
            skeleton.elevation[static_cast<std::size_t>(y) * site::kSiteWidth + x] =
                static_cast<std::uint16_t>(900U + y * 7U + x * 3U);
        }
    }
    auto edges = skeleton.edges;
    return {std::move(skeleton),
            std::move(edges),
            std::vector<site::SiteZoning>(site::kSiteTileCount, site::SiteZoning::Open),
            {},
            {},
            {},
            {},
            {},
            0};
}

[[nodiscard]] inline site::SiteProceduralLayer building_site_layer(
    site::SiteZoning zoning = site::SiteZoning::Residential) {
    auto result = open_site_layer();
    result.zoning[static_cast<std::size_t>(kLocalCenter.y) * site::kSiteWidth + kLocalCenter.x] =
        zoning;
    result.buildings.push_back({*test_ruleset().find_building(zoning == site::SiteZoning::Commercial
                                                                  ? "building.shop"
                                                                  : "building.row_house"),
                                {31, 31},
                                3,
                                3,
                                site::SiteBoundarySide::North,
                                site::ProceduralBuildingDamage::Intact});
    return result;
}

inline void set_site_edge(site::SiteProceduralLayer& layer, site::SiteXY first, site::SiteXY second,
                          rules::EdgeId edge) {
    const auto first_index = static_cast<std::size_t>(first.y) * site::kSiteWidth + first.x;
    const auto second_index = static_cast<std::size_t>(second.y) * site::kSiteWidth + second.x;
    spatial::BoundarySide first_side;
    spatial::BoundarySide second_side;
    if (second.x == first.x + 1U && second.y == first.y) {
        first_side = spatial::BoundarySide::East;
        second_side = spatial::BoundarySide::West;
    } else if (second.y == first.y + 1U && second.x == first.x) {
        first_side = spatial::BoundarySide::South;
        second_side = spatial::BoundarySide::North;
    } else {
        throw std::invalid_argument{"測試 Site edge 只接受東或南鄰格"};
    }
    layer.edges[first_index * 4U + static_cast<std::size_t>(first_side)] = edge;
    layer.edges[second_index * 4U + static_cast<std::size_t>(second_side)] = edge;
}

}  // namespace aetheria::tests

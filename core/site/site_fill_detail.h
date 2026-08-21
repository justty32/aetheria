#pragma once

// site_fill_detail.h 是 F1 分區與 F2 建築填充的內部共用介面。

#include "core/site/site_projection.h"

namespace aetheria::site::fill_detail {

struct BuildingPlacement {
    SiteXY origin;
    std::uint8_t width{};
    std::uint8_t height{};
};

[[nodiscard]] constexpr std::size_t tile_index(std::uint16_t x, std::uint16_t y) noexcept {
    return static_cast<std::size_t>(y) * kSiteWidth + x;
}

[[nodiscard]] constexpr SiteZoning zoning_for(rules::SiteFillZone zone) noexcept {
    return zone == rules::SiteFillZone::Residential ? SiteZoning::Residential
                                                    : SiteZoning::Commercial;
}

void assign_site_zones(SiteProceduralLayer& layer, const SiteFastVars& fast,
                       const rules::Ruleset& ruleset);
void fill_site_buildings(SiteProceduralLayer& layer, const SiteFastVars& fast,
                         const rules::Ruleset& ruleset);
[[nodiscard]] BuildingPlacement placement_for(const SiteBlock& block, SiteBoundarySide side,
                                              std::uint16_t offset,
                                              const rules::BuildingDef& def);
[[nodiscard]] bool valid_building_placement(const SiteProceduralLayer& layer,
                                            const std::vector<std::uint8_t>& occupied,
                                            const SiteBlock& block,
                                            const BuildingPlacement& placement,
                                            SiteBoundarySide side, SiteZoning zone);
void mark_building_occupied(std::vector<std::uint8_t>& occupied,
                            const BuildingPlacement& placement);

}  // namespace aetheria::site::fill_detail

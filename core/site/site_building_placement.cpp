#include "core/site/site_fill_detail.h"

namespace aetheria::site::fill_detail {
namespace {

[[nodiscard]] bool fits_block(const SiteBlock& block,
                              const BuildingPlacement& placement) noexcept {
    return placement.origin.x >= block.origin.x && placement.origin.y >= block.origin.y &&
           static_cast<std::uint32_t>(placement.origin.x) + placement.width <=
               static_cast<std::uint32_t>(block.origin.x) + block.width &&
           static_cast<std::uint32_t>(placement.origin.y) + placement.height <=
               static_cast<std::uint32_t>(block.origin.y) + block.height;
}

[[nodiscard]] bool has_street_frontage(const SiteSkeleton& skeleton,
                                       const BuildingPlacement& placement,
                                       SiteBoundarySide side) {
    const auto x_end = static_cast<std::uint16_t>(placement.origin.x + placement.width);
    const auto y_end = static_cast<std::uint16_t>(placement.origin.y + placement.height);
    if (side == SiteBoundarySide::North) {
        if (placement.origin.y == 0) {
            return false;
        }
        for (std::uint16_t x = placement.origin.x; x < x_end; ++x) {
            if (skeleton.roads[tile_index(x, placement.origin.y - 1U)] == 0) {
                return false;
            }
        }
    } else if (side == SiteBoundarySide::South) {
        if (y_end >= kSiteHeight) {
            return false;
        }
        for (std::uint16_t x = placement.origin.x; x < x_end; ++x) {
            if (skeleton.roads[tile_index(x, y_end)] == 0) {
                return false;
            }
        }
    } else if (side == SiteBoundarySide::West) {
        if (placement.origin.x == 0) {
            return false;
        }
        for (std::uint16_t y = placement.origin.y; y < y_end; ++y) {
            if (skeleton.roads[tile_index(placement.origin.x - 1U, y)] == 0) {
                return false;
            }
        }
    } else {
        if (x_end >= kSiteWidth) {
            return false;
        }
        for (std::uint16_t y = placement.origin.y; y < y_end; ++y) {
            if (skeleton.roads[tile_index(x_end, y)] == 0) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool footprint_is_free(const SiteProceduralLayer& layer,
                                     const std::vector<std::uint8_t>& occupied,
                                     const BuildingPlacement& placement, SiteZoning zone) {
    for (std::uint16_t y = placement.origin.y; y < placement.origin.y + placement.height; ++y) {
        for (std::uint16_t x = placement.origin.x; x < placement.origin.x + placement.width; ++x) {
            const auto index = tile_index(x, y);
            if (layer.skeleton.buildable[index] == 0 || layer.zoning[index] != zone ||
                occupied[index] != 0) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace

BuildingPlacement placement_for(const SiteBlock& block, SiteBoundarySide side,
                                std::uint16_t offset, const rules::BuildingDef& def) {
    if (side == SiteBoundarySide::North || side == SiteBoundarySide::South) {
        const auto y = side == SiteBoundarySide::North
                           ? block.origin.y
                           : static_cast<std::uint16_t>(block.origin.y + block.height - def.depth);
        return {{static_cast<std::uint16_t>(block.origin.x + offset), y}, def.frontage, def.depth};
    }
    const auto x = side == SiteBoundarySide::West
                       ? block.origin.x
                       : static_cast<std::uint16_t>(block.origin.x + block.width - def.depth);
    return {{x, static_cast<std::uint16_t>(block.origin.y + offset)}, def.depth, def.frontage};
}

bool valid_building_placement(const SiteProceduralLayer& layer,
                              const std::vector<std::uint8_t>& occupied,
                              const SiteBlock& block, const BuildingPlacement& placement,
                              SiteBoundarySide side, SiteZoning zone) {
    return fits_block(block, placement) && has_street_frontage(layer.skeleton, placement, side) &&
           footprint_is_free(layer, occupied, placement, zone);
}

void mark_building_occupied(std::vector<std::uint8_t>& occupied,
                            const BuildingPlacement& placement) {
    for (std::uint16_t y = placement.origin.y; y < placement.origin.y + placement.height; ++y) {
        for (std::uint16_t x = placement.origin.x; x < placement.origin.x + placement.width; ++x) {
            occupied[tile_index(x, y)] = UINT8_C(1);
        }
    }
}

}  // namespace aetheria::site::fill_detail

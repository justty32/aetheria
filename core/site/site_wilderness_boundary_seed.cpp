#include "core/site/site_wilderness_boundary_detail.h"

#include <stdexcept>

namespace aetheria::site::wilderness_boundary_detail {

std::size_t RegionBoundarySource::index(std::uint32_t x, std::uint32_t y) const {
    return tiles_.index_of(
        {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)});
}

std::uint16_t RegionBoundarySource::elevation(std::size_t index) const {
    return tiles_.elevation.at(index);
}

rules::GroundId RegionBoundarySource::ground(std::size_t index) const {
    const auto* mapping = ruleset_.terrain_ground_mapping(tiles_.base.at(index));
    if (mapping == nullptr) {
        throw std::runtime_error{"荒野邊界缺少 Terrain→Ground 映射"};
    }
    return mapping->ground;
}

std::uint8_t RegionBoundarySource::water_depth(std::size_t index) const {
    const auto* definition = ruleset_.ground(ground(index));
    return static_cast<std::uint8_t>(
        definition != nullptr && (definition->flags & rules::kGroundWaterFlag) != 0);
}

rules::EdgeId RegionBoundarySource::edge(std::size_t index,
                                         spatial::BoundarySide side) const {
    return tiles_.edges.at(index * 4U + static_cast<std::size_t>(side));
}

}  // namespace aetheria::site::wilderness_boundary_detail

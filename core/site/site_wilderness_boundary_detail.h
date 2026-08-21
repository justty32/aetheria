#pragma once

// site_wilderness_boundary_detail.h：把 RegionTiles 適配到共用邊界剖面生成介面。

#include "core/site/site_wilderness.h"

#include <cstddef>
#include <cstdint>

namespace aetheria::site::wilderness_boundary_detail {

class RegionBoundarySource {
public:
    RegionBoundarySource(const world::RegionTiles& tiles, const rules::Ruleset& ruleset)
        : tiles_{tiles}, ruleset_{ruleset} {}

    [[nodiscard]] std::uint32_t width() const noexcept { return tiles_.width; }
    [[nodiscard]] std::uint32_t height() const noexcept { return tiles_.height; }
    [[nodiscard]] std::size_t tile_count() const noexcept { return tiles_.tile_count(); }
    [[nodiscard]] std::size_t index(std::uint32_t x, std::uint32_t y) const;
    [[nodiscard]] std::uint16_t elevation(std::size_t index) const;
    [[nodiscard]] rules::GroundId ground(std::size_t index) const;
    [[nodiscard]] std::uint8_t water_depth(std::size_t index) const;
    [[nodiscard]] rules::EdgeId edge(std::size_t index, spatial::BoundarySide side) const;

private:
    const world::RegionTiles& tiles_;
    const rules::Ruleset& ruleset_;
};

}  // namespace aetheria::site::wilderness_boundary_detail

#pragma once

#include "core/site/site_wilderness.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace aetheria::site::wilderness_boundary_detail {

struct CornerSample {
    std::uint16_t elevation{};
    rules::GroundId ground{};
    std::uint8_t water_depth{};
};

[[nodiscard]] std::uint64_t corner_id(std::uint32_t x, std::uint32_t y,
                                      std::uint32_t width) noexcept;
[[nodiscard]] std::uint64_t internal_edge_id(std::size_t first, std::size_t second,
                                             std::uint32_t width) noexcept;
[[nodiscard]] CornerSample sample_corner(const world::RegionTiles& tiles, std::uint32_t x,
                                         std::uint32_t y, std::uint64_t seed,
                                         const rules::Ruleset& ruleset);
[[nodiscard]] std::optional<world::RegionXY> neighbor_of(const world::RegionTiles& tiles,
                                                         world::RegionXY coordinate,
                                                         SiteBoundarySide side);
[[nodiscard]] std::array<std::uint32_t, 4> edge_corners(world::RegionXY coordinate,
                                                        SiteBoundarySide side) noexcept;

}  // namespace aetheria::site::wilderness_boundary_detail

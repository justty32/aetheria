#include "core/site/site_wilderness_boundary_detail.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace aetheria::site::wilderness_boundary_detail {
namespace {

[[nodiscard]] std::vector<std::size_t> corner_tiles(const world::RegionTiles& tiles,
                                                    std::uint32_t corner_x,
                                                    std::uint32_t corner_y) {
    std::vector<std::size_t> result;
    result.reserve(4);
    for (std::int32_t dy = -1; dy <= 0; ++dy) {
        for (std::int32_t dx = -1; dx <= 0; ++dx) {
            const auto x = static_cast<std::int32_t>(corner_x) + dx;
            const auto y = static_cast<std::int32_t>(corner_y) + dy;
            if (x >= 0 && y >= 0 && x < static_cast<std::int32_t>(tiles.width) &&
                y < static_cast<std::int32_t>(tiles.height)) {
                result.push_back(tiles.index_of(
                    {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)}));
            }
        }
    }
    return result;
}

}  // namespace

std::uint64_t corner_id(std::uint32_t x, std::uint32_t y, std::uint32_t width) noexcept {
    return static_cast<std::uint64_t>(y) * (width + 1U) + x;
}

std::uint64_t internal_edge_id(std::size_t first, std::size_t second,
                               std::uint32_t width) noexcept {
    const auto low = std::min(first, second);
    const bool south = std::max(first, second) - low == width;
    return static_cast<std::uint64_t>(low) * 2U + static_cast<std::uint64_t>(south);
}

CornerSample sample_corner(const world::RegionTiles& tiles, std::uint32_t x, std::uint32_t y,
                           std::uint64_t seed, const rules::Ruleset& ruleset) {
    const auto candidates = corner_tiles(tiles, x, y);
    if (candidates.empty()) {
        throw std::logic_error{"荒野角點沒有相鄰 Region tile"};
    }
    std::uint64_t elevation_sum{};
    for (const auto index : candidates) {
        elevation_sum += tiles.elevation[index];
    }
    const auto terrain = tiles.base[candidates[static_cast<std::size_t>(seed % candidates.size())]];
    const auto* mapping = ruleset.terrain_ground_mapping(terrain);
    if (mapping == nullptr) {
        throw std::runtime_error{"荒野角點缺少 Terrain→Ground 映射"};
    }
    const auto* ground = ruleset.ground(mapping->ground);
    const bool water = ground != nullptr && (ground->flags & rules::kGroundWaterFlag) != 0;
    return {static_cast<std::uint16_t>(elevation_sum / candidates.size()), mapping->ground,
            static_cast<std::uint8_t>(water ? 1U : 0U)};
}

std::optional<world::RegionXY> neighbor_of(const world::RegionTiles& tiles,
                                           world::RegionXY coordinate,
                                           SiteBoundarySide side) {
    auto result = coordinate;
    switch (side) {
    case SiteBoundarySide::North:
        --result.y;
        break;
    case SiteBoundarySide::East:
        ++result.x;
        break;
    case SiteBoundarySide::South:
        ++result.y;
        break;
    case SiteBoundarySide::West:
        --result.x;
        break;
    }
    if (result.x < 0 || result.y < 0 || result.x >= static_cast<std::int32_t>(tiles.width) ||
        result.y >= static_cast<std::int32_t>(tiles.height)) {
        return std::nullopt;
    }
    return result;
}

std::array<std::uint32_t, 4> edge_corners(world::RegionXY coordinate,
                                          SiteBoundarySide side) noexcept {
    const auto x = static_cast<std::uint32_t>(coordinate.x);
    const auto y = static_cast<std::uint32_t>(coordinate.y);
    switch (side) {
    case SiteBoundarySide::North:
        return {x, y, x + 1U, y};
    case SiteBoundarySide::East:
        return {x + 1U, y, x + 1U, y + 1U};
    case SiteBoundarySide::South:
        return {x, y + 1U, x + 1U, y + 1U};
    case SiteBoundarySide::West:
        return {x, y, x, y + 1U};
    }
    return {};
}

}  // namespace aetheria::site::wilderness_boundary_detail

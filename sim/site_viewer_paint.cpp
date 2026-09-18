#include "sim/site_viewer.h"

#include "core/site/site_projection.h"
#include "core/site/site_wilderness.h"
#include "core/world/region_tiles.h"
#include "sim/debug_canvas.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "sim/site_viewer_detail.h"

namespace aetheria::sim::detail::site_viewer {

namespace {

[[nodiscard]] std::size_t tile_index(std::uint16_t x, std::uint16_t y) noexcept {
    return static_cast<std::size_t>(y) * site::kSiteWidth + x;
}

[[nodiscard]] DebugColor ground_color(rules::GroundId id, const rules::Ruleset& ruleset) {
    const auto* ground = ruleset.ground(id);
    if (ground == nullptr) {
        return {255, 0, 255};
    }
    if ((ground->flags & rules::kGroundWaterFlag) != 0) {
        return {42, 112, 180};
    }
    if (ground->id == "ground.grass") {
        return {112, 152, 82};
    }
    if (ground->id == "ground.sand") {
        return {210, 185, 118};
    }
    if (ground->id == "ground.mud") {
        return {116, 88, 64};
    }
    if (ground->id == "ground.tundra") {
        return {172, 186, 181};
    }
    return {139, 139, 139};
}

[[nodiscard]] std::optional<DebugColor> edge_color(rules::EdgeId id,
                                                   const rules::Ruleset& ruleset) {
    const auto* edge = ruleset.edge(id);
    if (edge == nullptr || edge->flags == 0) {
        return std::nullopt;
    }
    if ((edge->flags & rules::kEdgeGateFlag) != 0) {
        return DebugColor{255, 171, 37};
    }
    if ((edge->flags & rules::kEdgeTowerFlag) != 0) {
        return DebugColor{120, 48, 150};
    }
    if ((edge->flags & rules::kEdgeWallFlag) != 0) {
        return DebugColor{43, 32, 48};
    }
    if ((edge->flags & rules::kEdgeMoatFlag) != 0) {
        return DebugColor{36, 170, 210};
    }
    if ((edge->flags & rules::kEdgeRiverFlag) != 0) {
        return DebugColor{31, 96, 204};
    }
    if ((edge->flags & rules::kEdgeRoadFlag) != 0) {
        return DebugColor{111, 73, 48};
    }
    return DebugColor{230, 230, 230};
}

void paint_edge(DebugCanvas& canvas, std::uint16_t x, std::uint16_t y, spatial::BoundarySide side,
                DebugColor color) {
    const auto left = static_cast<std::int32_t>(x) * kTilePixels;
    const auto top = static_cast<std::int32_t>(y) * kTilePixels;
    switch (side) {
    case spatial::BoundarySide::North:
        canvas.draw_line(left, top, left + kTilePixels, top, 2, color);
        break;
    case spatial::BoundarySide::East:
        canvas.draw_line(left + kTilePixels, top, left + kTilePixels, top + kTilePixels, 2, color);
        break;
    case spatial::BoundarySide::South:
        canvas.draw_line(left, top + kTilePixels, left + kTilePixels, top + kTilePixels, 2, color);
        break;
    case spatial::BoundarySide::West:
        canvas.draw_line(left, top, left, top + kTilePixels, 2, color);
        break;
    }
}

[[nodiscard]] DebugColor block_color(std::size_t index) noexcept {
    const auto value = static_cast<std::uint32_t>((index + 11U) * UINT32_C(2246822519));
    return {static_cast<std::uint8_t>(80U + (value & 0x8FU)),
            static_cast<std::uint8_t>(80U + ((value >> 8U) & 0x8FU)),
            static_cast<std::uint8_t>(80U + ((value >> 16U) & 0x8FU))};
}

} // namespace

void paint_ground(DebugCanvas& canvas, const site::SiteSkeleton& skeleton,
                  const rules::Ruleset& ruleset) {
    for (std::uint16_t y = 0; y < site::kSiteHeight; ++y) {
        for (std::uint16_t x = 0; x < site::kSiteWidth; ++x) {
            canvas.fill_rect(x * kTilePixels, y * kTilePixels, kTilePixels, kTilePixels,
                             ground_color(skeleton.ground[tile_index(x, y)], ruleset));
        }
    }
}

void paint_roads(DebugCanvas& canvas, const site::SiteSkeleton& skeleton) {
    for (std::uint16_t y = 0; y < site::kSiteHeight; ++y) {
        for (std::uint16_t x = 0; x < site::kSiteWidth; ++x) {
            const auto index = tile_index(x, y);
            if (skeleton.roads[index] != 0) {
                canvas.fill_rect(x * kTilePixels, y * kTilePixels, kTilePixels, kTilePixels,
                                 {139, 94, 59});
            }
            if (skeleton.water[index] != 0) {
                canvas.fill_rect(x * kTilePixels, y * kTilePixels, kTilePixels, kTilePixels,
                                 {39, 115, 190});
            }
        }
    }
}

void paint_edges(DebugCanvas& canvas, const std::vector<rules::EdgeId>& edges,
                 const rules::Ruleset& ruleset) {
    for (std::uint16_t y = 0; y < site::kSiteHeight; ++y) {
        for (std::uint16_t x = 0; x < site::kSiteWidth; ++x) {
            const auto index = tile_index(x, y);
            for (std::size_t side = 0; side < 4; ++side) {
                if ((side == 1 && x + 1U != site::kSiteWidth) ||
                    (side == 2 && y + 1U != site::kSiteHeight)) {
                    continue;
                }
                const auto color = edge_color(edges[index * 4U + side], ruleset);
                if (color.has_value()) {
                    paint_edge(canvas, x, y, static_cast<spatial::BoundarySide>(side), *color);
                }
            }
        }
    }
}

void paint_blocks(DebugCanvas& canvas, const std::vector<site::SiteBlock>& blocks) {
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        const auto& block = blocks[index];
        canvas.fill_rect(block.origin.x * kTilePixels, block.origin.y * kTilePixels,
                         block.width * kTilePixels, block.height * kTilePixels, block_color(index));
    }
}

} // namespace aetheria::sim::detail::site_viewer

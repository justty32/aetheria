#include "sim/local_viewer.h"

#include "core/local/local_buildings.h"
#include "core/local/local_tiles.h"
#include "core/local/local_underground.h"
#include "sim/debug_canvas.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "sim/local_viewer_detail.h"

namespace aetheria::sim::detail::local_viewer {

namespace {

[[nodiscard]] std::size_t tile_index(std::uint16_t x, std::uint16_t y) noexcept {
    return static_cast<std::size_t>(y) * local::kLocalWidth + x;
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
    if ((edge->flags & rules::kEdgeWindowFlag) != 0) {
        return DebugColor{56, 219, 255};
    }
    if ((edge->flags & rules::kEdgeWallFlag) != 0) {
        return DebugColor{43, 32, 48};
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

[[nodiscard]] DebugColor room_color(std::size_t index) noexcept {
    const auto value = static_cast<std::uint32_t>((index + 1U) * UINT32_C(2654435761));
    return {static_cast<std::uint8_t>(90U + (value & 0x7FU)),
            static_cast<std::uint8_t>(90U + ((value >> 8U) & 0x7FU)),
            static_cast<std::uint8_t>(90U + ((value >> 16U) & 0x7FU))};
}

} // namespace

void paint_ground(DebugCanvas& canvas, const local::LocalTiles& tiles,
                  const rules::Ruleset& ruleset) {
    for (std::uint16_t y = 0; y < local::kLocalHeight; ++y) {
        for (std::uint16_t x = 0; x < local::kLocalWidth; ++x) {
            canvas.fill_rect(x * kTilePixels, y * kTilePixels, kTilePixels, kTilePixels,
                             ground_color(tiles.ground[tile_index(x, y)], ruleset));
        }
    }
}

void paint_edges(DebugCanvas& canvas, const local::LocalTiles& tiles,
                 const rules::Ruleset& ruleset) {
    for (std::uint16_t y = 0; y < local::kLocalHeight; ++y) {
        for (std::uint16_t x = 0; x < local::kLocalWidth; ++x) {
            const auto index = tile_index(x, y);
            for (std::size_t side = 0; side < 4; ++side) {
                if ((side == 1 && x + 1U != local::kLocalWidth) ||
                    (side == 2 && y + 1U != local::kLocalHeight)) {
                    continue;
                }
                const auto color = edge_color(tiles.edges[index * 4U + side], ruleset);
                if (color.has_value()) {
                    paint_edge(canvas, x, y, static_cast<spatial::BoundarySide>(side), *color);
                }
            }
        }
    }
}

void paint_rooms(DebugCanvas& canvas, const local::BuildingLocalSkeleton& skeleton, std::int8_t z) {
    for (std::size_t index = 0; index < skeleton.rooms.size(); ++index) {
        const auto& room = skeleton.rooms[index];
        if (room.z != z) {
            continue;
        }
        canvas.fill_rect(room.footprint.x * kTilePixels, room.footprint.y * kTilePixels,
                         room.footprint.width * kTilePixels, room.footprint.height * kTilePixels,
                         room_color(index));
    }
}

void paint_rooms(DebugCanvas& canvas, const local::UndergroundLocalSkeleton& skeleton,
                 std::int8_t z) {
    for (std::size_t index = 0; index < skeleton.rooms.size(); ++index) {
        const auto& room = skeleton.rooms[index];
        if (room.z != z) {
            continue;
        }
        canvas.fill_rect(room.footprint.x * kTilePixels, room.footprint.y * kTilePixels,
                         room.footprint.width * kTilePixels, room.footprint.height * kTilePixels,
                         room_color(index));
    }
}

void paint_occupants(DebugCanvas& canvas, const local::LocalTiles& tiles,
                     const local::BuildingLocalSkeleton* building, std::int8_t z) {
    std::vector<std::uint8_t> furniture(local::kLocalTileCount);
    if (building != nullptr) {
        for (const auto& item : building->furniture) {
            if (item.z == z) {
                furniture[tile_index(item.tile.x, item.tile.y)] = 1;
            }
        }
    }
    for (std::uint16_t y = 0; y < local::kLocalHeight; ++y) {
        for (std::uint16_t x = 0; x < local::kLocalWidth; ++x) {
            const auto index = tile_index(x, y);
            const auto left = static_cast<std::int32_t>(x) * kTilePixels;
            const auto top = static_cast<std::int32_t>(y) * kTilePixels;
            if (tiles.overlay[index] == local::OverlayId::Vegetation) {
                canvas.fill_rect(left + 2, top + 2, 4, 4, {31, 112, 52});
            } else if (tiles.overlay[index] == local::OverlayId::Stone) {
                canvas.fill_rect(left + 2, top + 2, 4, 4, {80, 88, 96});
            } else if (tiles.overlay[index] == local::OverlayId::ScatteredObject) {
                canvas.fill_rect(left + 2, top + 2, 4, 4, {212, 104, 42});
            } else if (tiles.overlay[index] == local::OverlayId::Road) {
                canvas.fill_rect(left + 1, top + 2, 7, 4, {132, 91, 58});
            } else if (tiles.overlay[index] == local::OverlayId::Stairs) {
                canvas.fill_rect(left + 1, top + 1, 6, 6, {245, 226, 75});
            }
            if (tiles.occupant[index] != 0) {
                canvas.fill_rect(left + 2, top + 2, 4, 4,
                                 furniture[index] != 0 ? DebugColor{32, 230, 220}
                                                       : DebugColor{246, 68, 146});
            }
        }
    }
}

} // namespace aetheria::sim::detail::local_viewer

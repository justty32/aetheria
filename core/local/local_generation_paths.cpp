// local_generation_paths.cpp：道路與河道 crossing 收集及路徑繪製。

#include "core/local/local_tiles.h"

#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>

#include "core/local/local_generation_detail.h"

namespace aetheria::local::detail::generation {
namespace {

void paint_path_brush(OpenLocalSkeleton& result, LocalXY center, std::uint8_t width,
                      std::uint32_t flags, rules::GroundId water_ground) {
    const auto half = static_cast<std::int32_t>(width / 2U);
    for (std::int32_t dy = -half; dy <= half; ++dy) {
        for (std::int32_t dx = -half; dx <= half; ++dx) {
            const auto x = static_cast<std::int32_t>(center.x) + dx;
            const auto y = static_cast<std::int32_t>(center.y) + dy;
            if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(kLocalWidth) ||
                y >= static_cast<std::int32_t>(kLocalHeight)) {
                continue;
            }
            const auto index = tile_index(static_cast<std::uint16_t>(x),
                                          static_cast<std::uint16_t>(y));
            if ((flags & rules::kEdgeRiverFlag) != 0) {
                result.tiles.ground[index] = water_ground;
            }
            if ((flags & rules::kEdgeRoadFlag) != 0) {
                result.tiles.overlay[index] = OverlayId::Road;
            }
        }
    }
}

void paint_axis_segment(OpenLocalSkeleton& result, LocalXY first, LocalXY last,
                        std::uint8_t width, std::uint32_t flags,
                        rules::GroundId water_ground) {
    const auto dx = first.x < last.x ? 1 : (first.x > last.x ? -1 : 0);
    const auto dy = first.y < last.y ? 1 : (first.y > last.y ? -1 : 0);
    auto x = static_cast<std::int32_t>(first.x);
    auto y = static_cast<std::int32_t>(first.y);
    while (true) {
        paint_path_brush(result,
                         {static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y)},
                         width, flags, water_ground);
        if (x == last.x && y == last.y) {
            break;
        }
        x += dx;
        y += dy;
    }
}

void route_path(OpenLocalSkeleton& result, const PathAnchor& anchor, LocalXY hub,
                std::uint32_t path_flag, rules::GroundId water_ground) {
    const bool vertical_entry = anchor.side == spatial::BoundarySide::North ||
                                anchor.side == spatial::BoundarySide::South;
    const LocalXY bend = vertical_entry ? LocalXY{anchor.tile.x, hub.y}
                                        : LocalXY{hub.x, anchor.tile.y};
    paint_axis_segment(result, anchor.tile, bend, anchor.width, path_flag, water_ground);
    paint_axis_segment(result, bend, hub, anchor.width, path_flag, water_ground);
}

}  // namespace

[[nodiscard]] std::vector<PathAnchor> collect_path_anchors(
    const OpenLocalSkeleton& skeleton, const rules::Ruleset& ruleset) {
    std::vector<PathAnchor> result;
    for (std::size_t side_index = 0; side_index < skeleton.boundaries.size(); ++side_index) {
        const auto side = static_cast<spatial::BoundarySide>(side_index);
        for (const auto& crossing : skeleton.boundaries[side_index].crossings) {
            const auto* definition = ruleset.edge(crossing.kind);
            if (definition != nullptr) {
                result.push_back(
                    {boundary_tile(side, crossing.pos), side, crossing.width, definition->flags});
            }
        }
    }
    return result;
}

void connect_crossings(OpenLocalSkeleton& result, rules::GroundId water_ground,
                       const rules::Ruleset& ruleset) {
    const auto anchors = collect_path_anchors(result, ruleset);
    for (std::size_t kind_index = 0; kind_index < kPathFlags.size(); ++kind_index) {
        const auto path_flag = kPathFlags[kind_index];
        const LocalXY hub{static_cast<std::uint16_t>(31U + kind_index),
                          static_cast<std::uint16_t>(31U + kind_index)};
        std::size_t crossing_count{};
        for (const auto& anchor : anchors) {
            if ((anchor.flags & path_flag) == 0) {
                continue;
            }
            route_path(result, anchor, hub, path_flag, water_ground);
            ++crossing_count;
        }
        if (crossing_count == 1U) {
            paint_path_brush(result, hub, 5, path_flag, water_ground);
            result.path_endpoints.push_back({hub, path_flag});
        }
    }
}

}  // namespace aetheria::local::detail::generation

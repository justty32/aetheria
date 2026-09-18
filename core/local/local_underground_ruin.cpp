// local_underground_ruin.cpp：路線 C 遺跡由路線 A 骨架拆除結構邊。

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>

#include "core/local/local_buildings.h"
#include "core/local/local_underground_detail.h"
#include "core/worldgen/region_seed.h"

namespace aetheria::local::underground_detail {
namespace {

struct EdgeRef {
    LocalXY tile;
    spatial::BoundarySide side{spatial::BoundarySide::North};
};

[[nodiscard]] bool structural_edge(rules::EdgeId edge, const rules::Ruleset& ruleset) {
    const auto* definition = ruleset.edge(edge);
    return definition != nullptr && (definition->flags & rules::kEdgeWallFlag) != 0;
}

[[nodiscard]] std::vector<EdgeRef> physical_structural_edges(const LocalTiles& tiles,
                                                             const rules::Ruleset& ruleset) {
    std::vector<EdgeRef> result;
    for (std::uint16_t y = 0; y < kLocalHeight; ++y) {
        for (std::uint16_t x = 0; x < kLocalWidth; ++x) {
            const LocalXY tile{x, y};
            const auto base = tile_index(tile) * kDirections;
            const auto append = [&](spatial::BoundarySide side) {
                if (structural_edge(tiles.edges[base + static_cast<std::size_t>(side)], ruleset)) {
                    result.push_back({tile, side});
                }
            };
            append(spatial::BoundarySide::East);
            append(spatial::BoundarySide::South);
            if (x == 0) {
                append(spatial::BoundarySide::West);
            }
            if (y == 0) {
                append(spatial::BoundarySide::North);
            }
        }
    }
    return result;
}

}  // namespace

LayerBuild build_ruin_layer(const LocalSlowVars& slow, LocalXY entrance, std::int8_t z,
                            std::uint64_t seed, const rules::Ruleset& ruleset,
                            std::uint32_t& original_segments, std::uint32_t& removed_segments) {
    auto route_a_slow = slow;
    route_a_slow.zoning = site::SiteZoning::Residential;
    const auto source = build_building_local_skeleton(route_a_slow, seed, ruleset);
    LayerBuild result{
        source.layers.at(0), std::vector<std::uint8_t>(kLocalTileCount, 1), {}, {}, entrance};
    std::ranges::fill(result.tiles.occupant, EntityId{});
    std::ranges::fill(result.tiles.overlay, OverlayId::None);
    for (const auto& room : source.rooms) {
        if (room.z == 0) {
            result.rooms.push_back({room.footprint, z});
        }
    }
    const auto none = *ruleset.find_edge("edge.none");
    auto original = physical_structural_edges(result.tiles, ruleset);
    original_segments += static_cast<std::uint32_t>(original.size());
    for (std::size_t index = 0; index < result.rooms.size(); ++index) {
        const auto path = orthogonal_path(entrance, room_center(result.rooms[index].footprint),
                                          ((seed >> (index % 32U)) & 1U) == 0);
        carve_path(result, path, none);
    }
    auto remaining = physical_structural_edges(result.tiles, ruleset);
    auto removed = original.size() - remaining.size();
    const auto percent = static_cast<std::size_t>(60U + seed % 21U);
    const auto target = (original.size() * percent + 99U) / 100U;
    std::ranges::sort(remaining, [&](const EdgeRef& lhs, const EdgeRef& rhs) {
        const auto key = [](const EdgeRef& edge) {
            return (static_cast<std::uint64_t>(edge.tile.y) << 24U) |
                   (static_cast<std::uint64_t>(edge.tile.x) << 8U) |
                   static_cast<std::uint8_t>(edge.side);
        };
        const auto lhs_key = key(lhs);
        const auto rhs_key = key(rhs);
        return std::pair{worldgen::splitmix64(seed ^ lhs_key), lhs_key} <
               std::pair{worldgen::splitmix64(seed ^ rhs_key), rhs_key};
    });
    for (const auto& edge : remaining) {
        if (removed >= target) {
            break;
        }
        if (structural_edge(result.tiles.edges[tile_index(edge.tile) * kDirections +
                                               static_cast<std::size_t>(edge.side)],
                            ruleset)) {
            set_edge(result.tiles, edge.tile, edge.side, none);
            ++removed;
        }
    }
    removed_segments += static_cast<std::uint32_t>(removed);
    result.exit = result.rooms.empty() ? entrance : room_center(result.rooms.back().footprint);
    return result;
}

}  // namespace aetheria::local::underground_detail

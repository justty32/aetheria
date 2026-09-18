// local_underground_geometry.cpp：路線 C 共用岩層、格掘進與走廊路徑操作。

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

[[nodiscard]] spatial::BoundarySide direction_between(LocalXY from, LocalXY to) {
    if (to.x == from.x + 1U && to.y == from.y) {
        return spatial::BoundarySide::East;
    }
    if (from.x == to.x + 1U && to.y == from.y) {
        return spatial::BoundarySide::West;
    }
    if (to.y == from.y + 1U && to.x == from.x) {
        return spatial::BoundarySide::South;
    }
    if (from.y == to.y + 1U && to.x == from.x) {
        return spatial::BoundarySide::North;
    }
    throw std::logic_error{"地下走廊只能連接四鄰格"};
}

void carve_step(LayerBuild& layer, LocalXY from, LocalXY to, rules::EdgeId none) {
    carve_cell(layer, from);
    carve_cell(layer, to);
    set_edge(layer.tiles, from, direction_between(from, to), none);
}

}  // namespace

[[nodiscard]] LocalTiles make_rock_layer(rules::GroundId ground, rules::EdgeId wall) {
    LocalTiles result;
    result.ground.assign(kLocalTileCount, ground);
    result.overlay.assign(kLocalTileCount, OverlayId::Stone);
    result.occupant.assign(kLocalTileCount, 0);
    result.edges.assign(kLocalTileCount * kDirections, wall);
    result.light.assign(kLocalTileCount, 48);
    return result;
}

void carve_cell(LayerBuild& layer, LocalXY tile) {
    layer.excavated[tile_index(tile)] = 1;
    layer.tiles.overlay[tile_index(tile)] = OverlayId::None;
}

[[nodiscard]] std::vector<LocalXY> orthogonal_path(LocalXY from, LocalXY to,
                                                   bool horizontal_first) {
    std::vector<LocalXY> result{from};
    auto current = from;
    const auto advance_x = [&] {
        while (current.x != to.x) {
            current.x = static_cast<std::uint16_t>(current.x + (current.x < to.x ? 1 : -1));
            result.push_back(current);
        }
    };
    const auto advance_y = [&] {
        while (current.y != to.y) {
            current.y = static_cast<std::uint16_t>(current.y + (current.y < to.y ? 1 : -1));
            result.push_back(current);
        }
    };
    if (horizontal_first) {
        advance_x();
        advance_y();
    } else {
        advance_y();
        advance_x();
    }
    return result;
}

void carve_path(LayerBuild& layer, const std::vector<LocalXY>& path, rules::EdgeId none) {
    carve_cell(layer, path.front());
    for (std::size_t index = 1; index < path.size(); ++index) {
        carve_step(layer, path[index - 1U], path[index], none);
    }
}

void set_edge(LocalTiles& tiles, LocalXY tile, spatial::BoundarySide side, rules::EdgeId edge) {
    tiles.edges[tile_index(tile) * kDirections + static_cast<std::size_t>(side)] = edge;
    auto neighbor = tile;
    auto opposite = spatial::BoundarySide::North;
    bool inside = true;
    switch (side) {
        case spatial::BoundarySide::North:
            inside = tile.y != 0;
            if (inside) {
                --neighbor.y;
            }
            opposite = spatial::BoundarySide::South;
            break;
        case spatial::BoundarySide::East:
            inside = tile.x + 1U < kLocalWidth;
            if (inside) {
                ++neighbor.x;
            }
            opposite = spatial::BoundarySide::West;
            break;
        case spatial::BoundarySide::South:
            inside = tile.y + 1U < kLocalHeight;
            if (inside) {
                ++neighbor.y;
            }
            opposite = spatial::BoundarySide::North;
            break;
        case spatial::BoundarySide::West:
            inside = tile.x != 0;
            if (inside) {
                --neighbor.x;
            }
            opposite = spatial::BoundarySide::East;
            break;
    }
    if (inside) {
        tiles.edges[tile_index(neighbor) * kDirections + static_cast<std::size_t>(opposite)] = edge;
    }
}

}  // namespace aetheria::local::underground_detail

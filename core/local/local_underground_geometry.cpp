// local_underground_geometry.cpp：路線 C 的單層掘進、房間走廊與路線 A 拆除。

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

[[nodiscard]] constexpr LocalXY room_center(const spatial::PartitionRect& room) noexcept {
    return {static_cast<std::uint16_t>(room.x + room.width / 2U),
            static_cast<std::uint16_t>(room.y + room.height / 2U)};
}

[[nodiscard]] constexpr bool contains(const spatial::PartitionRect& room, LocalXY tile) noexcept {
    return tile.x >= room.x && tile.y >= room.y && tile.x < room.x + room.width &&
           tile.y < room.y + room.height;
}

void carve_room(LayerBuild& layer, const spatial::PartitionRect& room, rules::EdgeId none) {
    for (std::uint16_t y = room.y; y < room.y + room.height; ++y) {
        for (std::uint16_t x = room.x; x < room.x + room.width; ++x) {
            const LocalXY tile{x, y};
            carve_cell(layer, tile);
            if (x + 1U < room.x + room.width) {
                set_edge(layer.tiles, tile, spatial::BoundarySide::East, none);
            }
            if (y + 1U < room.y + room.height) {
                set_edge(layer.tiles, tile, spatial::BoundarySide::South, none);
            }
        }
    }
}

[[nodiscard]] UndergroundCorridor corridor_record(const std::vector<LocalXY>& path,
                                                  const UndergroundRoom& destination, std::int8_t z,
                                                  std::uint16_t from, std::uint16_t to) {
    for (std::size_t index = 1; index < path.size(); ++index) {
        if (!contains(destination.footprint, path[index - 1U]) &&
            contains(destination.footprint, path[index])) {
            return {z,           from,
                    to,          path[index - 1U],
                    path[index], static_cast<std::uint16_t>(path.size())};
        }
    }
    throw std::logic_error{"地下走廊沒有進入目的房間"};
}

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

LayerBuild build_mine_layer(LocalXY entrance, std::int8_t z, std::uint64_t seed,
                            rules::GroundId ground, rules::EdgeId wall, rules::EdgeId none) {
    LayerBuild result{make_rock_layer(ground, wall),
                      std::vector<std::uint8_t>(kLocalTileCount),
                      {},
                      {},
                      entrance};
    carve_cell(result, entrance);
    const bool horizontal = (seed & 1U) == 0;
    const std::int32_t sign = ((seed >> 1U) & 1U) == 0 ? 1 : -1;
    auto current = entrance;
    std::vector<LocalXY> trunk{current};
    for (std::uint16_t step = 0; step < 44; ++step) {
        auto next = current;
        const auto drift = static_cast<std::int32_t>((seed >> ((step % 8U) * 8U)) & 3U) == 0
                               ? ((step / 8U) % 2U == 0 ? 1 : -1)
                               : 0;
        auto primary = horizontal ? static_cast<std::int32_t>(current.x)
                                  : static_cast<std::int32_t>(current.y);
        auto secondary = horizontal ? static_cast<std::int32_t>(current.y)
                                    : static_cast<std::int32_t>(current.x);
        primary += sign;
        secondary = std::clamp(secondary + drift, 3, 60);
        if (primary < 3 || primary > 60) {
            primary = horizontal ? static_cast<std::int32_t>(entrance.x)
                                 : static_cast<std::int32_t>(entrance.y);
            primary += (sign > 0 ? -1 : 1) * static_cast<std::int32_t>(step / 2U + 1U);
            primary = std::clamp(primary, 3, 60);
        }
        if (horizontal) {
            next = {static_cast<std::uint16_t>(primary), static_cast<std::uint16_t>(secondary)};
        } else {
            next = {static_cast<std::uint16_t>(secondary), static_cast<std::uint16_t>(primary)};
        }
        const auto path = orthogonal_path(current, next, ((seed >> (step % 32U)) & 1U) == 0);
        carve_path(result, path, none);
        current = next;
        trunk.push_back(current);
    }
    result.corridors.push_back({z, std::numeric_limits<std::uint16_t>::max(),
                                std::numeric_limits<std::uint16_t>::max(), entrance, current,
                                static_cast<std::uint16_t>(trunk.size())});
    constexpr std::array<std::uint16_t, 5> branch_lengths{12, 10, 8, 6, 4};
    for (std::size_t branch = 0; branch < branch_lengths.size(); ++branch) {
        const auto start = trunk[7U + branch * 7U];
        auto end = start;
        const auto branch_sign = ((seed >> (branch + 12U)) & 1U) == 0 ? 1 : -1;
        if (horizontal) {
            end.y = static_cast<std::uint16_t>(std::clamp(
                static_cast<std::int32_t>(start.y) + branch_sign * branch_lengths[branch], 2, 61));
        } else {
            end.x = static_cast<std::uint16_t>(std::clamp(
                static_cast<std::int32_t>(start.x) + branch_sign * branch_lengths[branch], 2, 61));
        }
        const auto path = orthogonal_path(start, end, horizontal);
        carve_path(result, path, none);
        result.corridors.push_back({z, std::numeric_limits<std::uint16_t>::max(),
                                    std::numeric_limits<std::uint16_t>::max(), start, end,
                                    static_cast<std::uint16_t>(path.size())});
    }
    result.exit = current;
    return result;
}

LayerBuild build_dungeon_layer(LocalXY entrance, std::int8_t z, std::uint64_t seed,
                               rules::GroundId ground, rules::EdgeId wall, rules::EdgeId none) {
    LayerBuild result{make_rock_layer(ground, wall),
                      std::vector<std::uint8_t>(kLocalTileCount),
                      {},
                      {},
                      entrance};
    result.rooms.push_back({{static_cast<std::uint16_t>(entrance.x - 4U),
                             static_cast<std::uint16_t>(entrance.y - 4U), 9, 9},
                            z});
    constexpr std::array<std::uint16_t, 4> cell_x{3, 18, 33, 48};
    constexpr std::array<std::uint16_t, 2> cell_y{5, 43};
    for (std::size_t row = 0; row < cell_y.size(); ++row) {
        for (std::size_t order = 0; order < cell_x.size(); ++order) {
            const auto column = row == 0 ? order : cell_x.size() - order - 1U;
            const auto mixed = worldgen::splitmix64(seed ^ (row << 8U) ^ column);
            const auto width = static_cast<std::uint16_t>(7U + mixed % 4U);
            const auto height = static_cast<std::uint16_t>(7U + (mixed >> 8U) % 4U);
            const auto x = static_cast<std::uint16_t>(cell_x[column] + (mixed >> 16U) % 3U);
            const auto y = static_cast<std::uint16_t>(cell_y[row] + (mixed >> 24U) % 3U);
            result.rooms.push_back({{x, y, width, height}, z});
        }
    }
    for (const auto& room : result.rooms) {
        carve_room(result, room.footprint, none);
    }
    for (std::size_t index = 1; index < result.rooms.size(); ++index) {
        const auto path = orthogonal_path(room_center(result.rooms[index - 1U].footprint),
                                          room_center(result.rooms[index].footprint),
                                          ((seed >> (index % 32U)) & 1U) == 0);
        carve_path(result, path, none);
        result.corridors.push_back(corridor_record(path, result.rooms[index], z,
                                                   static_cast<std::uint16_t>(index - 1U),
                                                   static_cast<std::uint16_t>(index)));
    }
    // 下行梯保留在入口房，避免下一層入口房與固定末端房重疊形成可達性旁路。
    result.exit = entrance;
    return result;
}

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

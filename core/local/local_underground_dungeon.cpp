// local_underground_dungeon.cpp：路線 C 地城單層房間與房間間走廊。

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

}  // namespace

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

}  // namespace aetheria::local::underground_detail

#include <algorithm>
#include <cstdint>
#include <vector>

#include "core/local/local_building_detail.h"
#include "core/worldgen/region_seed.h"

namespace aetheria::local::detail {
namespace {

constexpr std::uint64_t kRoomSalt = UINT64_C(0x947C02D5A613EB8F);
constexpr std::uint64_t kDoorSalt = UINT64_C(0xC2A7184E39B065DF);

void paint_outer_walls(LocalTiles& tiles, spatial::PartitionRect rect, rules::EdgeId wall) {
    for (std::uint16_t x = rect.x; x < rect.x + rect.width; ++x) {
        set_edge(tiles, {x, rect.y}, spatial::BoundarySide::North, wall);
        set_edge(tiles, {x, static_cast<std::uint16_t>(rect.y + rect.height - 1U)},
                 spatial::BoundarySide::South, wall);
    }
    for (std::uint16_t y = rect.y; y < rect.y + rect.height; ++y) {
        set_edge(tiles, {rect.x, y}, spatial::BoundarySide::West, wall);
        set_edge(tiles, {static_cast<std::uint16_t>(rect.x + rect.width - 1U), y},
                 spatial::BoundarySide::East, wall);
    }
}

void paint_cut(LocalTiles& tiles, const spatial::PartitionCut& cut, rules::EdgeId wall,
               rules::EdgeId door, std::uint16_t& door_count) {
    for (std::uint16_t offset = 0; offset < cut.extent; ++offset) {
        if (cut.vertical) {
            set_edge(tiles,
                     {static_cast<std::uint16_t>(cut.coordinate - 1U),
                      static_cast<std::uint16_t>(cut.start + offset)},
                     spatial::BoundarySide::East, wall);
        } else {
            set_edge(tiles,
                     {static_cast<std::uint16_t>(cut.start + offset),
                      static_cast<std::uint16_t>(cut.coordinate - 1U)},
                     spatial::BoundarySide::South, wall);
        }
    }
    const auto midpoint = static_cast<std::uint16_t>(cut.start + cut.extent / 2U);
    if (cut.vertical) {
        set_edge(tiles, {static_cast<std::uint16_t>(cut.coordinate - 1U), midpoint},
                 spatial::BoundarySide::East, door);
    } else {
        set_edge(tiles, {midpoint, static_cast<std::uint16_t>(cut.coordinate - 1U)},
                 spatial::BoundarySide::South, door);
    }
    ++door_count;
}

void open_frontage(LocalTiles& tiles, const LocalHouse& house, rules::EdgeId door,
                   rules::EdgeId window, bool include_door, bool include_windows,
                   std::uint64_t seed, std::uint16_t& door_count,
                   std::uint16_t& window_count) {
    const auto& rect = house.footprint;
    const bool horizontal = house.frontage == spatial::BoundarySide::North ||
                            house.frontage == spatial::BoundarySide::South;
    const auto start = horizontal ? rect.x : rect.y;
    const auto extent = horizontal ? rect.width : rect.height;
    const auto door_position = static_cast<std::uint16_t>(
        start + 1U + worldgen::splitmix64(seed ^ kDoorSalt) % (extent - 2U));
    auto edge_tile = [&](std::uint16_t position) {
        switch (house.frontage) {
            case spatial::BoundarySide::North:
                return LocalXY{position, rect.y};
            case spatial::BoundarySide::East:
                return LocalXY{static_cast<std::uint16_t>(rect.x + rect.width - 1U), position};
            case spatial::BoundarySide::South:
                return LocalXY{position, static_cast<std::uint16_t>(rect.y + rect.height - 1U)};
            case spatial::BoundarySide::West:
                return LocalXY{rect.x, position};
        }
        return LocalXY{};
    };
    if (include_door) {
        set_edge(tiles, edge_tile(door_position), house.frontage, door);
        ++door_count;
    }
    if (!include_windows) {
        return;
    }
    for (auto position = static_cast<std::uint16_t>(start + 2U); position + 1U < start + extent;
         position = static_cast<std::uint16_t>(position + 4U)) {
        if (position == door_position) {
            continue;
        }
        set_edge(tiles, edge_tile(position), house.frontage, window);
        ++window_count;
    }
}

[[nodiscard]] rules::LocalRoomKind room_kind(site::SiteZoning zoning,
                                             std::size_t room_index) noexcept {
    if (zoning == site::SiteZoning::Commercial) {
        return room_index == 0 ? rules::LocalRoomKind::Shop : rules::LocalRoomKind::Workshop;
    }
    return room_index % 4U == 0 ? rules::LocalRoomKind::Kitchen : rules::LocalRoomKind::Bedroom;
}

}  // namespace

void build_floor(BuildingLocalSkeleton& result, const LocalSlowVars& slow,
                 std::uint16_t house_index, std::int8_t z, std::uint64_t seed,
                 const rules::Ruleset& ruleset) {
    const auto& config = ruleset.local_building_rules();
    auto& tiles = result.layers.at(z);
    const auto& house = result.houses[house_index];
    const auto door = slow.zoning == site::SiteZoning::Commercial ? config.commercial_door_edge
                                                                  : config.residential_door_edge;
    paint_outer_walls(tiles, house.footprint, config.wall_edge);
    const auto partition =
        spatial::partition_rect(house.footprint, seed ^ kRoomSalt,
                                {config.room_split_depth, config.room_cut_min_percent,
                                 config.room_cut_max_percent, config.room_min_extent, 0});
    for (std::size_t index = 0; index < partition.leaves.size(); ++index) {
        result.rooms.push_back(
            {partition.leaves[index], z, house_index, room_kind(slow.zoning, index)});
    }
    for (const auto& cut : partition.cuts) {
        paint_cut(tiles, cut, config.wall_edge, door, result.door_count);
    }
    open_frontage(tiles, house, door, config.window_edge, z == 0, z >= 0, seed,
                  result.door_count, result.window_count);
}

}  // namespace aetheria::local::detail

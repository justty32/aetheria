#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "core/local/local_building_detail.h"
#include "core/worldgen/region_seed.h"

namespace aetheria::local {
namespace {

[[nodiscard]] bool room_has_door(const LocalRoom& room, const LocalTiles& tiles,
                                 const rules::Ruleset& ruleset) noexcept {
    const auto& rect = room.footprint;
    const auto is_door = [&](LocalXY tile, spatial::BoundarySide side) {
        const auto edge = tiles.edges[detail::tile_index(tile.x, tile.y) * detail::kDirections +
                                      static_cast<std::size_t>(side)];
        const auto* definition = ruleset.edge(edge);
        constexpr auto flags = rules::kEdgeGateFlag | rules::kEdgeOpenableFlag;
        return definition != nullptr && (definition->flags & flags) == flags;
    };
    for (std::uint16_t x = rect.x; x < rect.x + rect.width; ++x) {
        if (is_door({x, rect.y}, spatial::BoundarySide::North) ||
            is_door({x, static_cast<std::uint16_t>(rect.y + rect.height - 1U)},
                    spatial::BoundarySide::South)) {
            return true;
        }
    }
    for (std::uint16_t y = rect.y; y < rect.y + rect.height; ++y) {
        if (is_door({rect.x, y}, spatial::BoundarySide::West) ||
            is_door({static_cast<std::uint16_t>(rect.x + rect.width - 1U), y},
                    spatial::BoundarySide::East)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] constexpr bool contains(const spatial::PartitionRect& outer,
                                      const spatial::PartitionRect& inner) noexcept {
    return inner.x >= outer.x && inner.y >= outer.y &&
           inner.x + inner.width <= outer.x + outer.width &&
           inner.y + inner.height <= outer.y + outer.height;
}

[[nodiscard]] bool symmetric_edges(const LocalTiles& tiles) noexcept {
    for (std::uint16_t y = 0; y < kLocalHeight; ++y) {
        for (std::uint16_t x = 0; x < kLocalWidth; ++x) {
            const auto index = detail::tile_index(x, y) * detail::kDirections;
            if (x + 1U < kLocalWidth) {
                const auto east =
                    detail::tile_index(static_cast<std::uint16_t>(x + 1U), y) * detail::kDirections;
                if (tiles.edges[index + static_cast<std::size_t>(spatial::BoundarySide::East)] !=
                    tiles.edges[east + static_cast<std::size_t>(spatial::BoundarySide::West)]) {
                    return false;
                }
            }
            if (y + 1U < kLocalHeight) {
                const auto south =
                    detail::tile_index(x, static_cast<std::uint16_t>(y + 1U)) * detail::kDirections;
                if (tiles.edges[index + static_cast<std::size_t>(spatial::BoundarySide::South)] !=
                    tiles.edges[south + static_cast<std::size_t>(spatial::BoundarySide::North)]) {
                    return false;
                }
            }
        }
    }
    return true;
}

}  // namespace

bool valid_building_invariants(const BuildingLocalSkeleton& skeleton,
                               const rules::Ruleset& ruleset) noexcept {
    if (!skeleton.valid_layout() || skeleton.houses.empty() || skeleton.rooms.empty() ||
        skeleton.door_count == 0 || skeleton.window_count == 0 || skeleton.furniture.empty() ||
        skeleton.resident_statistics == 0) {
        return false;
    }
    const auto all_edges_valid = std::ranges::all_of(skeleton.layers, [&](const auto& layer) {
        return symmetric_edges(layer.second) &&
               std::ranges::all_of(layer.second.edges, [&](rules::EdgeId edge) {
                   const auto* definition = ruleset.edge(edge);
                   if (definition == nullptr) {
                       return false;
                   }
                   const bool door =
                       (definition->flags & (rules::kEdgeGateFlag | rules::kEdgeOpenableFlag)) != 0;
                   return !door || (definition->flags & rules::kEdgeWallFlag) != 0;
               });
    });
    if (!all_edges_valid) {
        return false;
    }
    const auto residents = std::ranges::fold_left(
        skeleton.houses, std::uint32_t{},
        [](std::uint32_t sum, const LocalHouse& house) { return sum + house.resident_count; });
    if (residents != skeleton.resident_statistics) {
        return false;
    }
    const auto materialized_residents = std::ranges::fold_left(
        skeleton.houses, std::uint32_t{}, [](std::uint32_t sum, const LocalHouse& house) {
            return sum + (house.residents_materialized ? house.resident_count : 0U);
        });
    if (materialized_residents != skeleton.ambient_resident_count) {
        return false;
    }
    const auto expected_links = std::ranges::fold_left(
        skeleton.houses, std::size_t{}, [](std::size_t sum, const LocalHouse& house) {
            return sum + static_cast<std::size_t>(house.has_cellar) +
                   static_cast<std::size_t>(house.has_upper_floor);
        });
    if (expected_links != skeleton.vertical_links.size()) {
        return false;
    }
    for (const auto& house : skeleton.houses) {
        if (house.footprint.width == 0 || house.footprint.height == 0 ||
            house.footprint.x + house.footprint.width > kLocalWidth ||
            house.footprint.y + house.footprint.height > kLocalHeight) {
            return false;
        }
    }
    for (const auto& room : skeleton.rooms) {
        if (room.house >= skeleton.houses.size() || room.footprint.width < 5U ||
            room.footprint.height < 5U || !skeleton.layers.contains(room.z) ||
            !contains(skeleton.houses[room.house].footprint, room.footprint) ||
            !room_has_door(room, skeleton.layers.at(room.z), ruleset)) {
            return false;
        }
    }
    for (const auto& item : skeleton.furniture) {
        const auto* definition = ruleset.furniture(item.def);
        if (item.z != 0 || item.room >= skeleton.rooms.size() || definition == nullptr ||
            item.entity == 0 || skeleton.rooms[item.room].z != item.z ||
            definition->room != skeleton.rooms[item.room].kind ||
            item.tile.x < skeleton.rooms[item.room].footprint.x ||
            item.tile.y < skeleton.rooms[item.room].footprint.y ||
            item.tile.x >=
                skeleton.rooms[item.room].footprint.x + skeleton.rooms[item.room].footprint.width ||
            item.tile.y >= skeleton.rooms[item.room].footprint.y +
                               skeleton.rooms[item.room].footprint.height ||
            skeleton.layers.at(0).occupant[detail::tile_index(item.tile.x, item.tile.y)] !=
                item.entity) {
            return false;
        }
    }
    return std::ranges::all_of(skeleton.vertical_links, [&](const VerticalLink& link) {
        return link.from_z == 0 && (link.to_z == -1 || link.to_z == 1) &&
               skeleton.layers.contains(link.to_z) &&
               skeleton.layers.at(0).overlay[detail::tile_index(link.tile.x, link.tile.y)] ==
                   OverlayId::Stairs &&
               skeleton.layers.at(link.to_z)
                       .overlay[detail::tile_index(link.tile.x, link.tile.y)] == OverlayId::Stairs;
    });
}

}  // namespace aetheria::local

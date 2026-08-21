#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "core/local/local_building_detail.h"
#include "core/worldgen/region_seed.h"

namespace aetheria::local {
namespace {

constexpr std::uint64_t kFurnitureSalt = UINT64_C(0x325F8B91D6A40CE7);
constexpr std::uint64_t kResidentSalt = UINT64_C(0xB6E9C21374D50FA8);

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

void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= UINT64_C(1099511628211);
}

template <typename Value>
void hash_integer(std::uint64_t& hash, Value value) noexcept {
    const auto bits = static_cast<std::make_unsigned_t<Value>>(value);
    for (std::size_t byte = 0; byte < sizeof(bits); ++byte) {
        hash_byte(hash, static_cast<std::uint8_t>(bits >> (byte * 8U)));
    }
}

void hash_rect(std::uint64_t& hash, const spatial::PartitionRect& rect) noexcept {
    hash_integer(hash, rect.x);
    hash_integer(hash, rect.y);
    hash_integer(hash, rect.width);
    hash_integer(hash, rect.height);
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

namespace detail {

void fill_furniture(BuildingLocalSkeleton& result, std::uint64_t local_seed,
                    const rules::Ruleset& ruleset) {
    auto& ground = result.layers.at(0);
    for (std::size_t room_index = 0; room_index < result.rooms.size(); ++room_index) {
        const auto& room = result.rooms[room_index];
        if (room.z != 0) {
            continue;
        }
        for (std::size_t definition_index = 0; definition_index < ruleset.furniture().size();
             ++definition_index) {
            const auto& definition = ruleset.furniture()[definition_index];
            if (definition.room != room.kind) {
                continue;
            }
            const auto seed = worldgen::splitmix64(local_seed ^ kFurnitureSalt ^ room_index ^
                                                   (definition_index << 32U));
            const auto span = definition.maximum - definition.minimum + 1U;
            const auto wanted = static_cast<std::uint8_t>(definition.minimum + seed % span);
            const auto area = static_cast<std::size_t>(room.footprint.area());
            const auto start = static_cast<std::size_t>((seed >> 16U) % area);
            std::uint8_t placed{};
            for (std::size_t offset = 0; offset < area && placed < wanted; ++offset) {
                const auto position = (start + offset) % area;
                const auto x =
                    static_cast<std::uint16_t>(room.footprint.x + position % room.footprint.width);
                const auto y =
                    static_cast<std::uint16_t>(room.footprint.y + position / room.footprint.width);
                const auto index = tile_index(x, y);
                if (ground.occupant[index] != 0 || ground.overlay[index] == OverlayId::Stairs) {
                    continue;
                }
                const auto entity = UINT64_C(0x4000000000000000) |
                                    static_cast<std::uint64_t>(result.furniture.size() + 1U);
                ground.occupant[index] = entity;
                ground.overlay[index] = OverlayId::Furniture;
                result.furniture.push_back({{x, y},
                                            0,
                                            static_cast<std::uint16_t>(room_index),
                                            static_cast<rules::FurnitureDefId>(definition_index),
                                            entity});
                ++placed;
            }
        }
    }
}

}  // namespace detail

void materialize_ambient_residents(BuildingLocalSkeleton& skeleton, std::uint16_t house_index,
                                   std::uint64_t local_seed) {
    if (house_index >= skeleton.houses.size()) {
        throw std::out_of_range{"Local 居住者具象化的房屋下標超界"};
    }
    auto& house = skeleton.houses[house_index];
    if (house.residents_materialized) {
        return;
    }
    auto& tiles = skeleton.layers.at(0);
    const auto first_room = std::ranges::find_if(skeleton.rooms, [&](const LocalRoom& room) {
        return room.z == 0 && room.house == house_index;
    });
    if (first_room == skeleton.rooms.end()) {
        throw std::logic_error{"Local 房屋缺少地面層房間"};
    }
    const auto& rect = first_room->footprint;
    const auto area = static_cast<std::size_t>(rect.area());
    const auto seed = worldgen::splitmix64(local_seed ^ kResidentSalt ^ house_index);
    const auto start = static_cast<std::size_t>(seed % area);
    std::uint16_t placed{};
    for (std::size_t offset = 0; offset < area && placed < house.resident_count; ++offset) {
        const auto position = (start + offset) % area;
        const auto x = static_cast<std::uint16_t>(rect.x + position % rect.width);
        const auto y = static_cast<std::uint16_t>(rect.y + position / rect.width);
        const auto index = detail::tile_index(x, y);
        if (tiles.occupant[index] != 0) {
            continue;
        }
        const auto entity = UINT64_C(0x8000000000000000) |
                            (static_cast<std::uint64_t>(house_index) << 16U) |
                            (static_cast<std::uint64_t>(placed) + 1U);
        tiles.occupant[index] = entity;
        ++placed;
    }
    if (placed != house.resident_count) {
        throw std::logic_error{"Local 房間沒有足夠空格具象化居民"};
    }
    house.residents_materialized = true;
    skeleton.ambient_resident_count =
        static_cast<std::uint16_t>(skeleton.ambient_resident_count + placed);
}

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

std::uint64_t hash_building_local_skeleton(const BuildingLocalSkeleton& skeleton) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    hash_integer(hash, static_cast<std::uint64_t>(skeleton.layers.size()));
    for (const auto& [z, tiles] : skeleton.layers) {
        hash_byte(hash, static_cast<std::uint8_t>(z));
        for (const auto value : tiles.ground) {
            hash_integer(hash, rules::value_of(value));
        }
        for (const auto value : tiles.overlay) {
            hash_integer(hash, static_cast<std::uint16_t>(value));
        }
        for (const auto value : tiles.occupant) {
            hash_integer(hash, value);
        }
        for (const auto value : tiles.edges) {
            hash_integer(hash, rules::value_of(value));
        }
        for (const auto value : tiles.light) {
            hash_byte(hash, value);
        }
    }
    for (const auto value : skeleton.elevation) {
        hash_integer(hash, value);
    }
    for (const auto& house : skeleton.houses) {
        hash_rect(hash, house.footprint);
        hash_byte(hash, static_cast<std::uint8_t>(house.frontage));
        hash_integer(hash, house.resident_count);
        hash_byte(hash, house.has_cellar);
        hash_byte(hash, house.has_upper_floor);
        hash_byte(hash, house.residents_materialized);
    }
    for (const auto& room : skeleton.rooms) {
        hash_rect(hash, room.footprint);
        hash_byte(hash, static_cast<std::uint8_t>(room.z));
        hash_integer(hash, room.house);
        hash_byte(hash, static_cast<std::uint8_t>(room.kind));
    }
    for (const auto& item : skeleton.furniture) {
        hash_integer(hash, item.tile.x);
        hash_integer(hash, item.tile.y);
        hash_byte(hash, static_cast<std::uint8_t>(item.z));
        hash_integer(hash, item.room);
        hash_integer(hash, rules::value_of(item.def));
        hash_integer(hash, item.entity);
    }
    for (const auto& link : skeleton.vertical_links) {
        hash_integer(hash, link.tile.x);
        hash_integer(hash, link.tile.y);
        hash_byte(hash, static_cast<std::uint8_t>(link.from_z));
        hash_byte(hash, static_cast<std::uint8_t>(link.to_z));
    }
    hash_integer(hash, skeleton.resident_statistics);
    hash_integer(hash, skeleton.door_count);
    hash_integer(hash, skeleton.window_count);
    hash_integer(hash, skeleton.ambient_resident_count);
    return hash;
}

}  // namespace aetheria::local

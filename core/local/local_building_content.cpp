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

}  // namespace aetheria::local

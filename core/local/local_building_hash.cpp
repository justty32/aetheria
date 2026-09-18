#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "core/local/local_building_detail.h"
#include "core/worldgen/region_seed.h"

namespace aetheria::local {
namespace {

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

}  // namespace

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

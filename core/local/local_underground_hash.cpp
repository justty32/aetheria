// local_underground_hash.cpp：路線 C 地下骨架的正規化雜湊。

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/local/local_underground.h"
#include "core/local/local_underground_detail.h"

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

void hash_tile(std::uint64_t& hash, LocalXY tile) noexcept {
    hash_integer(hash, tile.x);
    hash_integer(hash, tile.y);
}

void hash_rect(std::uint64_t& hash, const spatial::PartitionRect& rect) noexcept {
    hash_integer(hash, rect.x);
    hash_integer(hash, rect.y);
    hash_integer(hash, rect.width);
    hash_integer(hash, rect.height);
}

}  // namespace

std::uint64_t hash_underground_local_skeleton(const UndergroundLocalSkeleton& skeleton) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    hash_byte(hash, static_cast<std::uint8_t>(skeleton.kind));
    hash_byte(hash, skeleton.depth);
    hash_tile(hash, skeleton.entrance);
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
    for (const auto& [z, mask] : skeleton.excavated) {
        hash_byte(hash, static_cast<std::uint8_t>(z));
        for (const auto value : mask) {
            hash_byte(hash, value);
        }
    }
    for (const auto& room : skeleton.rooms) {
        hash_rect(hash, room.footprint);
        hash_byte(hash, static_cast<std::uint8_t>(room.z));
    }
    for (const auto& corridor : skeleton.corridors) {
        hash_byte(hash, static_cast<std::uint8_t>(corridor.z));
        hash_integer(hash, corridor.from_room);
        hash_integer(hash, corridor.to_room);
        hash_tile(hash, corridor.destination_outside);
        hash_tile(hash, corridor.destination_inside);
        hash_integer(hash, corridor.tile_count);
    }
    for (const auto& link : skeleton.vertical_links) {
        hash_tile(hash, link.tile);
        hash_byte(hash, static_cast<std::uint8_t>(link.upper_z));
        hash_byte(hash, static_cast<std::uint8_t>(link.lower_z));
    }
    hash_integer(hash, skeleton.excavated_count);
    hash_integer(hash, skeleton.ruin_original_segments);
    hash_integer(hash, skeleton.ruin_removed_segments);
    return hash;
}

}  // namespace aetheria::local

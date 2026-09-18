// dungeon_rooms.cpp：地城房間幾何生成。

#include "core/local/dungeon.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "core/rules/power.h"
#include "core/worldgen/region_seed.h"

#include "core/local/dungeon_detail.h"

namespace aetheria::local::detail::dungeon {

[[nodiscard]] std::vector<DungeonRoom> generate_rooms(
    std::uint64_t seed, std::uint8_t depth, const rules::DungeonArchetypeRules& profile) {
    std::vector<DungeonRoom> rooms;
    rooms.reserve(profile.room_count);
    for (std::uint8_t index = 0; index < profile.room_count; ++index) {
        const auto random = worldgen::splitmix64(
            seed ^ (static_cast<std::uint64_t>(depth) << 48U) ^
            (static_cast<std::uint64_t>(index) << 32U));
        const auto width = static_cast<std::uint8_t>(6U + random % 5U);
        const auto height = static_cast<std::uint8_t>(6U + (random >> 8U) % 5U);
        const bool mirrored = index % 2U != 0U && !rooms.empty() &&
                              (random >> 16U) % 100U < profile.symmetry_percent;
        LocalXY origin;
        if (mirrored) {
            const auto& previous = rooms.back();
            origin.x = static_cast<std::uint16_t>(kLocalWidth - previous.origin.x - width);
            origin.y = previous.origin.y;
        } else {
            origin.x = static_cast<std::uint16_t>(2U + (random >> 24U) % (60U - width));
            origin.y = static_cast<std::uint16_t>(2U + (random >> 40U) % (60U - height));
        }
        rooms.push_back({
            .origin = origin,
            .width = width,
            .height = height,
            .natural = (random >> 20U) % 100U < profile.natural_percent,
            .eroded = (random >> 52U) % 100U < profile.erosion_percent,
        });
    }
    return rooms;
}

}  // namespace aetheria::local::detail::dungeon

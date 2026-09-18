// local_underground_mine.cpp：路線 C 礦坑單層主幹與分支掘進。

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

}  // namespace aetheria::local::underground_detail

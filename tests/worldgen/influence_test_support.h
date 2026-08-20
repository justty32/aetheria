#pragma once

// influence_test_support.h 提供影響力測試的 owner hash、單源成本與故障注入順序版。

#include "core/world/region_movement.h"
#include "core/worldgen/influence_spread.h"
#include "tests/world/region_test_support.h"

#include <array>
#include <cstdint>
#include <limits>
#include <queue>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace aetheria::tests {

[[nodiscard]] inline std::uint64_t owner_hash(std::span<const world::FactionId> owners) {
    auto hash = UINT64_C(14695981039346656037);
    for (const auto owner : owners) {
        auto value = static_cast<std::uint16_t>(owner);
        for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
            hash ^= static_cast<std::uint8_t>(value & UINT8_MAX);
            hash *= UINT64_C(1099511628211);
            value >>= 8U;
        }
    }
    return hash;
}

[[nodiscard]] inline std::vector<std::int64_t>
single_source_costs(const world::RegionTiles& tiles, world::RegionXY source,
                    std::int64_t budget) {
    using Entry = std::pair<std::int64_t, std::size_t>;
    std::vector<std::int64_t> costs(
        tiles.tile_count(), std::numeric_limits<std::int64_t>::max());
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
    const auto source_index = tiles.index_of(source);
    costs[source_index] = 0;
    open.emplace(0, source_index);
    constexpr std::array<std::pair<int, int>, 4> kDirections{{
        {0, -1}, {1, 0}, {0, 1}, {-1, 0},
    }};
    while (!open.empty()) {
        const auto [cost, index] = open.top();
        open.pop();
        if (cost != costs[index]) {
            continue;
        }
        const auto x = static_cast<int>(index % tiles.width);
        const auto y = static_cast<int>(index / tiles.width);
        const world::RegionXY here{static_cast<std::int16_t>(x),
                                   static_cast<std::int16_t>(y)};
        for (const auto& [dx, dy] : kDirections) {
            const auto nx = x + dx;
            const auto ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= static_cast<int>(tiles.width) ||
                ny >= static_cast<int>(tiles.height)) {
                continue;
            }
            const world::RegionXY next{static_cast<std::int16_t>(nx),
                                       static_cast<std::int16_t>(ny)};
            const auto next_index = tiles.index_of(next);
            const auto* terrain = test_ruleset().terrain(tiles.base[next_index]);
            if (terrain == nullptr) {
                throw std::runtime_error{"測試地圖含不存在的 TerrainId"};
            }
            if ((terrain->flags & rules::kTerrainWaterFlag) != 0) {
                continue;
            }
            const auto step = world::region_step_cost(tiles, here, next, test_ruleset(), 1);
            if (cost <= budget - step && cost + step < costs[next_index]) {
                costs[next_index] = cost + step;
                open.emplace(costs[next_index], next_index);
            }
        }
    }
    return costs;
}

[[nodiscard]] inline std::vector<world::FactionId>
sequential_first_wins(const world::RegionTiles& tiles,
                      std::span<const worldgen::InfluenceCapital> capitals,
                      std::int64_t budget) {
    std::vector<world::FactionId> owners(tiles.tile_count(), world::FactionId{0});
    std::vector<std::int64_t> best(tiles.tile_count(),
                                   std::numeric_limits<std::int64_t>::max());
    for (const auto& capital : capitals) {
        const auto costs = single_source_costs(tiles, capital.tile, budget);
        for (std::size_t index = 0; index < costs.size(); ++index) {
            if (costs[index] <= budget && costs[index] < best[index]) {
                best[index] = costs[index];
                owners[index] = capital.faction;
            }
        }
    }
    return owners;
}

}  // namespace aetheria::tests

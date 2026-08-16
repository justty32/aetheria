// region_path.cpp：Region 內四鄰接 A* 尋路（原屬 region_movement.cpp）。

#include "core/world/region_movement.h"
#include "core/world/region_movement_detail.h"

#include <algorithm>
#include <array>
#include <limits>
#include <queue>
#include <stdexcept>
#include <tuple>

namespace aetheria::world {
namespace {

[[nodiscard]] std::array<RegionXY, 4> neighbors(RegionXY tile) noexcept {
    return {{{tile.x, static_cast<std::int16_t>(tile.y - 1)},
             {static_cast<std::int16_t>(tile.x + 1), tile.y},
             {tile.x, static_cast<std::int16_t>(tile.y + 1)},
             {static_cast<std::int16_t>(tile.x - 1), tile.y}}};
}

}  // namespace

std::optional<RegionPath> find_region_path(const RegionTiles& tiles, RegionXY start, RegionXY goal,
                                           const rules::Ruleset& ruleset, std::uint8_t season,
                                           std::uint32_t heuristic_multiplier) {
    if (!detail::passable(tiles, start, ruleset) || !detail::passable(tiles, goal, ruleset)) {
        return std::nullopt;
    }
    const auto count = tiles.tile_count();
    const auto start_index = tiles.index_of(start);
    const auto goal_index = tiles.index_of(goal);
    constexpr auto infinity = std::numeric_limits<std::int64_t>::max();
    constexpr auto missing = std::numeric_limits<std::size_t>::max();
    std::vector<std::int64_t> distance(count, infinity);
    std::vector<std::size_t> parent(count, missing);
    using Candidate = std::tuple<std::int64_t, std::int64_t, std::size_t>;
    std::priority_queue<Candidate, std::vector<Candidate>, std::greater<>> open;
    const auto minimum = minimum_region_step_cost(ruleset, season);
    distance[start_index] = 0;
    open.emplace(static_cast<std::int64_t>(detail::manhattan(start, goal)) * minimum *
                     heuristic_multiplier,
                 0, start_index);

    while (!open.empty()) {
        const auto [estimate, known_cost, current] = open.top();
        static_cast<void>(estimate);
        open.pop();
        if (known_cost != distance[current]) {
            continue;
        }
        if (current == goal_index) {
            break;
        }
        const RegionXY from{static_cast<std::int16_t>(current % tiles.width),
                            static_cast<std::int16_t>(current / tiles.width)};
        for (const auto to : neighbors(from)) {
            if (!detail::passable(tiles, to, ruleset)) {
                continue;
            }
            const auto next = tiles.index_of(to);
            const auto candidate = known_cost + region_step_cost(tiles, from, to, ruleset, season);
            if (candidate >= distance[next]) {
                continue;
            }
            distance[next] = candidate;
            parent[next] = current;
            const auto heuristic = static_cast<std::int64_t>(detail::manhattan(to, goal)) * minimum *
                                   heuristic_multiplier;
            open.emplace(candidate + heuristic, candidate, next);
        }
    }
    if (distance[goal_index] == infinity) {
        return std::nullopt;
    }
    RegionPath result{{}, distance[goal_index]};
    for (auto current = goal_index;; current = parent[current]) {
        result.tiles.push_back({static_cast<std::int16_t>(current % tiles.width),
                                static_cast<std::int16_t>(current / tiles.width)});
        if (current == start_index) {
            break;
        }
        if (parent[current] == missing) {
            throw std::runtime_error{"A* parent chain 中斷"};
        }
    }
    std::ranges::reverse(result.tiles);
    return result;
}

}  // namespace aetheria::world

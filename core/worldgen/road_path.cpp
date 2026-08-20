#include "core/worldgen/road_path.h"

#include "core/worldgen/civ_tiles.h"

#include <algorithm>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>

namespace aetheria::worldgen::detail {
namespace {

[[nodiscard]] std::int64_t engineering_step_cost(const world::RegionTiles& tiles,
                                                 const RiverStageOutput& rivers, std::size_t from,
                                                 std::size_t to, const rules::Ruleset& ruleset,
                                                 const rules::CivilizationRules& civilization) {
    const auto* terrain = ruleset.terrain(tiles.base[to]);
    const auto* relief = ruleset.relief(tiles.relief[to]);
    if (terrain == nullptr || relief == nullptr ||
        (terrain->flags & rules::kTerrainWaterFlag) != 0) {
        return std::numeric_limits<std::int64_t>::max();
    }
    auto cost = static_cast<std::int64_t>(civilization.road_base_cost) +
                static_cast<std::int64_t>(terrain->move_cost + relief->move_cost) *
                    civilization.road_terrain_weight;
    const auto slope =
        std::abs(static_cast<int>(tiles.elevation[from]) - static_cast<int>(tiles.elevation[to]));
    cost += static_cast<std::int64_t>(slope / civilization.road_slope_divisor) *
            civilization.road_slope_weight;
    if (tiles.base[to] == civilization.swamp_terrain) {
        cost += civilization.road_swamp_penalty;
    }
    if (rivers.river_class[from] != 0 || rivers.river_class[to] != 0) {
        cost -= std::min<std::int64_t>(cost - 1, civilization.road_valley_discount);
    }
    const auto edge =
        tiles.edge_between(coordinate(from, tiles.width), coordinate(to, tiles.width));
    const auto* edge_definition = ruleset.edge(edge);
    if (edge_definition == nullptr) {
        throw std::runtime_error{"道路工程遇到無效 EdgeId"};
    }
    if ((edge_definition->flags & rules::kEdgeRiverFlag) != 0) {
        cost += civilization.road_river_crossing_penalty;
    }
    if (edge == civilization.history.road_edge) {
        cost = std::max<std::int64_t>(
            1, cost * civilization.history.ancient_road_reuse_numerator /
                   civilization.history.ancient_road_reuse_denominator);
    } else if ((edge_definition->flags & rules::kEdgeRoadFlag) != 0) {
        cost = std::max<std::int64_t>(1, cost * civilization.road_reuse_numerator /
                                             civilization.road_reuse_denominator);
    }
    return cost;
}

}  // namespace

std::optional<rules::EdgeId>
compound_edge(const rules::CivilizationRules& civilization, rules::EdgeId river,
              rules::EdgeId road) {
    for (const auto& crossing : civilization.crossings) {
        if (crossing.river == river && crossing.road == road) {
            return crossing.result;
        }
    }
    return std::nullopt;
}

std::optional<rules::EdgeId>
underlying_river(const rules::CivilizationRules& civilization, rules::EdgeId edge,
                 const rules::Ruleset& ruleset) {
    const auto* definition = ruleset.edge(edge);
    if (definition == nullptr || (definition->flags & rules::kEdgeRiverFlag) == 0) {
        return std::nullopt;
    }
    for (const auto& crossing : civilization.crossings) {
        if (crossing.result == edge) {
            return crossing.river;
        }
    }
    return edge;
}

std::size_t directed_offset(std::size_t from, std::size_t to, std::uint32_t width) {
    if (to + width == from) {
        return from * 4U;
    }
    if (to == from + 1U) {
        return from * 4U + 1U;
    }
    if (to == from + width) {
        return from * 4U + 2U;
    }
    if (to + 1U == from) {
        return from * 4U + 3U;
    }
    throw std::runtime_error{"道路 path 含非四鄰接 step"};
}

std::vector<std::size_t>
find_engineering_path(const world::RegionTiles& tiles, const RiverStageOutput& rivers,
                      std::size_t start, std::size_t goal, const rules::Ruleset& ruleset,
                      const rules::CivilizationRules& civilization) {
    const auto count = tiles.tile_count();
    constexpr auto infinity = std::numeric_limits<std::int64_t>::max();
    std::vector<std::int64_t> distance(count, infinity);
    std::vector<std::size_t> parent(count, kMissing);
    using Candidate = std::pair<std::int64_t, std::size_t>;
    std::priority_queue<Candidate, std::vector<Candidate>, std::greater<>> open;
    distance[start] = 0;
    open.push({0, start});
    while (!open.empty()) {
        const auto [known, current] = open.top();
        open.pop();
        if (known != distance[current]) {
            continue;
        }
        if (current == goal) {
            break;
        }
        for (const auto next : neighbors(current, tiles.width, tiles.height)) {
            if (next >= count) {
                continue;
            }
            const auto step =
                engineering_step_cost(tiles, rivers, current, next, ruleset, civilization);
            if (step == infinity || known > infinity - step || known + step >= distance[next]) {
                continue;
            }
            distance[next] = known + step;
            parent[next] = current;
            open.push({distance[next], next});
        }
    }
    if (distance[goal] == infinity) {
        throw std::runtime_error{"城市道路沒有可行工程路徑"};
    }
    std::vector<std::size_t> result;
    for (auto current = goal;; current = parent[current]) {
        result.push_back(current);
        if (current == start) {
            break;
        }
        if (parent[current] == kMissing) {
            throw std::runtime_error{"道路 parent chain 中斷"};
        }
    }
    std::ranges::reverse(result);
    return result;
}

}  // namespace aetheria::worldgen::detail

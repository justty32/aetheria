#include "core/worldgen/road_loops.h"

#include "core/world/region_movement.h"

#include <algorithm>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace aetheria::worldgen::detail {
namespace {

struct DisjointSet {
    explicit DisjointSet(std::size_t count) : parent(count), rank(count) {
        std::iota(parent.begin(), parent.end(), 0U);
    }

    [[nodiscard]] std::size_t find(std::size_t value) {
        if (parent[value] != value) {
            parent[value] = find(parent[value]);
        }
        return parent[value];
    }

    [[nodiscard]] bool unite(std::size_t lhs, std::size_t rhs) {
        lhs = find(lhs);
        rhs = find(rhs);
        if (lhs == rhs) {
            return false;
        }
        if (rank[lhs] < rank[rhs]) {
            std::swap(lhs, rhs);
        }
        parent[rhs] = lhs;
        if (rank[lhs] == rank[rhs]) {
            ++rank[lhs];
        }
        return true;
    }

    std::vector<std::size_t> parent;
    std::vector<std::uint8_t> rank;
};

[[nodiscard]] std::int64_t tree_distance(std::size_t start, std::size_t goal,
                                         const std::vector<CandidateConnection>& tree,
                                         std::size_t city_count) {
    std::vector<std::vector<std::pair<std::size_t, std::int64_t>>> graph(city_count);
    for (const auto& edge : tree) {
        graph[edge.first].push_back({edge.second, edge.cost});
        graph[edge.second].push_back({edge.first, edge.cost});
    }
    std::vector<std::int64_t> distance(city_count, -1);
    std::queue<std::size_t> open;
    distance[start] = 0;
    open.push(start);
    while (!open.empty()) {
        const auto current = open.front();
        open.pop();
        if (current == goal) {
            return distance[current];
        }
        for (const auto [next, cost] : graph[current]) {
            if (distance[next] < 0) {
                distance[next] = distance[current] + cost;
                open.push(next);
            }
        }
    }
    throw std::runtime_error{"MST tree distance 不連通"};
}

}  // namespace

SpanningTreeResult
build_minimum_spanning_tree(const world::RegionTiles& tiles,
                            const std::vector<CitySite>& ordered_cities,
                            const rules::Ruleset& ruleset) {
    SpanningTreeResult result;
    for (std::size_t first = 0; first < ordered_cities.size(); ++first) {
        for (std::size_t second = first + 1; second < ordered_cities.size(); ++second) {
            const auto path = world::find_region_path(tiles, ordered_cities[first].tile,
                                                      ordered_cities[second].tile, ruleset, 1);
            if (path.has_value()) {
                result.candidates.push_back({first, second, path->cost, false});
            }
        }
    }
    std::ranges::sort(result.candidates, [&](const auto& lhs, const auto& rhs) {
        return std::tuple{lhs.cost, ordered_cities[lhs.first].canonical_id,
                          ordered_cities[lhs.second].canonical_id} <
               std::tuple{rhs.cost, ordered_cities[rhs.first].canonical_id,
                          ordered_cities[rhs.second].canonical_id};
    });
    DisjointSet sets{ordered_cities.size()};
    for (auto& candidate : result.candidates) {
        if (sets.unite(candidate.first, candidate.second)) {
            candidate.selected = true;
            result.tree.push_back(candidate);
        }
    }
    if (result.tree.size() + 1U != ordered_cities.size()) {
        throw std::runtime_error{"城市完全圖無法連通所有城市"};
    }
    return result;
}

std::vector<CandidateConnection>
select_loop_connections(const std::vector<CandidateConnection>& candidates,
                        const std::vector<CandidateConnection>& tree,
                        const std::vector<CitySite>& ordered_cities,
                        const rules::CivilizationRules& civilization,
                        const RoadGenerationConfig& config) {
    struct LoopCandidate {
        std::size_t index{};
        std::int64_t tree_cost{};
    };
    std::vector<LoopCandidate> loops;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        if (!candidates[index].selected) {
            loops.push_back({index, tree_distance(candidates[index].first, candidates[index].second,
                                                  tree, ordered_cities.size())});
        }
    }
    std::ranges::sort(loops, [&](const LoopCandidate& lhs, const LoopCandidate& rhs) {
        const auto& lhs_edge = candidates[lhs.index];
        const auto& rhs_edge = candidates[rhs.index];
        const auto lhs_ratio = lhs.tree_cost * rhs_edge.cost;
        const auto rhs_ratio = rhs.tree_cost * lhs_edge.cost;
        if (lhs_ratio != rhs_ratio) {
            return lhs_ratio > rhs_ratio;
        }
        return std::tuple{lhs_edge.cost, ordered_cities[lhs_edge.first].canonical_id,
                          ordered_cities[lhs_edge.second].canonical_id} <
               std::tuple{rhs_edge.cost, ordered_cities[rhs_edge.first].canonical_id,
                          ordered_cities[rhs_edge.second].canonical_id};
    });
    const auto loop_percent = config.loop_percent_override == 0 ? civilization.loop_percent
                                                                : config.loop_percent_override;
    if (loop_percent < 10 || loop_percent > 20) {
        throw std::invalid_argument{"道路環路比例必須在 10..20"};
    }
    const auto loop_count = std::min<std::size_t>(
        loops.size(), std::max<std::size_t>(1, (tree.size() * loop_percent + 99U) / 100U));
    std::vector<CandidateConnection> selected;
    for (std::size_t index = 0; index < loop_count; ++index) {
        auto edge = candidates[loops[index].index];
        edge.selected = true;
        selected.push_back(edge);
    }
    return selected;
}

}  // namespace aetheria::worldgen::detail

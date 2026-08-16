#pragma once

// 城市道路 MST 建立與補環路選擇的內部 helper，供 road_network.cpp 的
// generate_roads 使用，從 civilization_generator.cpp 拆出。

#include "core/worldgen/region_generator.h"

#include "core/rules/ruleset.h"
#include "core/world/region_tiles.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace aetheria::worldgen::detail {

// CandidateConnection 是兩座 canonical 排序城市間的候選連線與其道路成本。
struct CandidateConnection {
    std::size_t first{};
    std::size_t second{};
    std::int64_t cost{};
    bool selected{};
};

// SpanningTreeResult 帶排序後的完整候選邊，以及 Kruskal 選出的連通樹子集。
struct SpanningTreeResult {
    std::vector<CandidateConnection> candidates;
    std::vector<CandidateConnection> tree;
};

[[nodiscard]] SpanningTreeResult
build_minimum_spanning_tree(const world::RegionTiles& tiles,
                            const std::vector<CitySite>& ordered_cities,
                            const rules::Ruleset& ruleset);

[[nodiscard]] std::vector<CandidateConnection>
select_loop_connections(const std::vector<CandidateConnection>& candidates,
                        const std::vector<CandidateConnection>& tree,
                        const std::vector<CitySite>& ordered_cities,
                        const rules::CivilizationRules& civilization,
                        const RoadGenerationConfig& config);

}  // namespace aetheria::worldgen::detail

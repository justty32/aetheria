// core/rules/ruleset_load_crossings.cpp：河級 × 道路級渡河複合 edge 查表的完整性驗證。

#include "core/rules/ruleset.h"

#include <array>
#include <set>
#include <stdexcept>
#include <utility>

namespace aetheria::rules {

void RulesetLoader::load_crossing_rules(const Ruleset& result, CivilizationRules& rules) {
    std::set<std::pair<std::uint16_t, std::uint16_t>> crossing_keys;
    const std::array river_names{"edge.stream", "edge.river", "edge.great_river"};
    for (const auto road : rules.road_edges) {
        const auto* road_definition = result.edge(road);
        if (road_definition == nullptr ||
            (road_definition->flags & kEdgeRoadFlag) == 0) {
            throw std::runtime_error{"civilization.toml road_edges 不是道路 def"};
        }
    }
    for (const auto& crossing : rules.crossings) {
        const auto key = std::pair{value_of(crossing.river), value_of(crossing.road)};
        const auto* compound = result.edge(crossing.result);
        if (!crossing_keys.insert(key).second || compound == nullptr ||
            (compound->flags & (kEdgeRoadFlag | kEdgeRiverFlag | kEdgeBridgeFlag)) !=
                (kEdgeRoadFlag | kEdgeRiverFlag | kEdgeBridgeFlag)) {
            throw std::runtime_error{"civilization.toml crossing 重複或不是複合 def"};
        }
    }
    for (const auto river_name : river_names) {
        const auto river = result.find_edge(river_name);
        if (!river.has_value()) {
            throw std::runtime_error{"civilization.toml 缺少標準河流 def"};
        }
        for (const auto road : rules.road_edges) {
            if (!crossing_keys.contains({value_of(*river), value_of(road)})) {
                throw std::runtime_error{"civilization.toml 河級 × 道路級查表不完整"};
            }
        }
    }
    rules.loaded = true;
}

}  // namespace aetheria::rules

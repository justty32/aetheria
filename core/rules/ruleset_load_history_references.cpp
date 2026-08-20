// core/rules/ruleset_load_history_references.cpp：上古道路與廢墟 def 引用的載入。

#include "core/rules/ruleset_load_history_detail.h"
#include "core/rules/toml_read.h"

#include <optional>
#include <set>
#include <stdexcept>
#include <string>

namespace aetheria::rules::detail {
namespace {

template <typename Id, typename Finder>
[[nodiscard]] Id require_history_reference(const toml::table& history, std::string_view field,
                                           const std::filesystem::path& path, Finder&& find) {
    const auto string_id = require_string(history, field, path);
    const auto id = find(string_id);
    if (!id.has_value()) {
        throw std::runtime_error{"civilization.toml history 引用不存在：" + string_id};
    }
    return *id;
}

}  // namespace

void load_history_references(CivilizationRules::HistoryRules& history,
                             const toml::table& history_table,
                             const std::filesystem::path& civilization_path,
                             const Ruleset& ruleset) {
    history.road_edge = require_history_reference<EdgeId>(
        history_table, "road_edge", civilization_path,
        [&ruleset](std::string_view id) { return ruleset.find_edge(id); });
    const auto* road = ruleset.edge(history.road_edge);
    const auto standard_road_id = ruleset.find_edge("edge.road");
    const auto* standard_road =
        standard_road_id.has_value() ? ruleset.edge(*standard_road_id) : nullptr;
    if (road == nullptr || standard_road == nullptr || (road->flags & kEdgeRoadFlag) == 0 ||
        (road->flags & (kEdgeRiverFlag | kEdgeBridgeFlag)) != 0 ||
        road->move_cost <= standard_road->move_cost) {
        throw std::runtime_error{
            "civilization.toml history road_edge 必須是純道路且移動成本高於 edge.road"};
    }

    const auto* ruins = history_table["ruin_features"].as_array();
    if (ruins == nullptr || ruins->size() != history.ruin_features.size()) {
        throw std::runtime_error{"civilization.toml history ruin_features 必須有三級"};
    }
    std::set<std::uint16_t> ruin_ids;
    for (std::size_t index = 0; index < history.ruin_features.size(); ++index) {
        const auto string_id = (*ruins)[index].value<std::string>();
        const auto feature =
            string_id.has_value() ? ruleset.find_feature(*string_id) : std::nullopt;
        const auto* definition = feature.has_value() ? ruleset.feature(*feature) : nullptr;
        if (definition == nullptr || (definition->flags & kFeatureRuinFlag) == 0) {
            throw std::runtime_error{
                "civilization.toml history ruin_features 引用不存在或不是廢墟 def"};
        }
        if (!ruin_ids.insert(value_of(*feature)).second) {
            throw std::runtime_error{"civilization.toml history ruin_features 三級 ID 不得重複"};
        }
        history.ruin_features[index] = *feature;
    }
}

}  // namespace aetheria::rules::detail

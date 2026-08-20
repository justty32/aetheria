// core/rules/ruleset_load_history.cpp：上古選址、古道與廢墟規則的載入。

#include "core/rules/ruleset.h"
#include "core/rules/toml_read.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <functional>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>

namespace aetheria::rules {

using namespace detail;
namespace {

[[nodiscard]] std::uint16_t require_history_u16(const toml::table& history,
                                                std::string_view field,
                                                const std::filesystem::path& path,
                                                bool allow_zero) {
    const auto value = require_integer(history, field, path);
    if (value < (allow_zero ? 0 : 1) || value > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error{"civilization.toml history 整數欄位超出範圍：" +
                                 std::string{field}};
    }
    return static_cast<std::uint16_t>(value);
}

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

void RulesetLoader::load_history_rules(Ruleset& result,
                                       const std::filesystem::path& data_directory) {
    const auto civilization_path = data_directory / "civilization.toml";
    if (!std::filesystem::is_regular_file(civilization_path)) {
        return;
    }

    toml::table civilization;
    try {
        civilization = toml::parse_file(civilization_path.string());
    } catch (const toml::parse_error& error) {
        throw std::runtime_error{"Ruleset TOML 格式錯誤：" + civilization_path.string() +
                                 "：" + std::string{error.description()}};
    }
    const auto* history_table = civilization["history"].as_table();
    if (history_table == nullptr) {
        throw std::runtime_error{"civilization.toml 缺少 [history]"};
    }

    auto& history = result.civilization_rules_.history;
    auto& weights = history.scoring_weights;
    weights.freshwater = require_int32(*history_table, "freshwater_weight", civilization_path);
    weights.farmland = require_int32(*history_table, "farmland_weight", civilization_path);
    weights.harbor = require_int32(*history_table, "harbor_weight", civilization_path);
    weights.defense = require_int32(*history_table, "defense_weight", civilization_path);
    weights.resource = require_int32(*history_table, "resource_weight", civilization_path);
    weights.bottleneck = require_int32(*history_table, "bottleneck_weight", civilization_path);
    weights.extreme_climate_penalty =
        require_int32(*history_table, "extreme_climate_penalty", civilization_path);
    weights.high_elevation_penalty =
        require_int32(*history_table, "high_elevation_penalty", civilization_path);
    history.ancient_site_count =
        require_history_u16(*history_table, "ancient_site_count", civilization_path, true);
    history.ancient_city_count =
        require_history_u16(*history_table, "ancient_city_count", civilization_path, true);
    history.ancient_town_count =
        require_history_u16(*history_table, "ancient_town_count", civilization_path, true);
    history.ancient_road_reuse_numerator = require_history_u16(
        *history_table, "ancient_road_reuse_numerator", civilization_path, false);
    history.ancient_road_reuse_denominator = require_history_u16(
        *history_table, "ancient_road_reuse_denominator", civilization_path, false);

    const auto survivor_percent =
        require_history_u16(*history_table, "survivor_percent", civilization_path, false);
    if (survivor_percent > 100) {
        throw std::runtime_error{"civilization.toml history survivor_percent 必須介於 1 與 100"};
    }
    history.survivor_percent = static_cast<std::uint8_t>(survivor_percent);

    history.ancient_site_bonus =
        require_int32(*history_table, "ancient_site_bonus", civilization_path);
    if (history.ancient_site_bonus < 0) {
        throw std::runtime_error{"civilization.toml history ancient_site_bonus 不得為負"};
    }

    const auto* spacing = (*history_table)["minimum_spacing"].as_array();
    if (spacing == nullptr || spacing->size() != history.minimum_spacing.size()) {
        throw std::runtime_error{"civilization.toml history minimum_spacing 必須有三級"};
    }
    for (std::size_t index = 0; index < history.minimum_spacing.size(); ++index) {
        const auto value = (*spacing)[index].value<std::int64_t>();
        if (!value.has_value() || *value <= 0 ||
            *value > std::numeric_limits<std::uint16_t>::max()) {
            throw std::runtime_error{"civilization.toml history minimum_spacing 值無效"};
        }
        history.minimum_spacing[index] = static_cast<std::uint16_t>(*value);
    }

    const auto& civilization_rules = result.civilization_rules_;
    const auto spacing_is_larger =
        std::ranges::equal(history.minimum_spacing, civilization_rules.minimum_spacing,
                           std::ranges::greater{});
    if (!std::is_sorted(history.minimum_spacing.begin(), history.minimum_spacing.end()) ||
        history.ancient_road_reuse_numerator > history.ancient_road_reuse_denominator ||
        (history.ancient_site_count != 0 &&
         (history.ancient_site_count >= civilization_rules.target_city_count ||
          !spacing_is_larger ||
          static_cast<std::uint32_t>(history.ancient_city_count) + history.ancient_town_count >
              history.ancient_site_count))) {
        throw std::runtime_error{"civilization.toml history 的間距、數量或古道折扣無效"};
    }

    history.road_edge = require_history_reference<EdgeId>(
        *history_table, "road_edge", civilization_path,
        [&result](std::string_view id) { return result.find_edge(id); });
    const auto* road = result.edge(history.road_edge);
    const auto standard_road_id = result.find_edge("edge.road");
    const auto* standard_road =
        standard_road_id.has_value() ? result.edge(*standard_road_id) : nullptr;
    if (road == nullptr || standard_road == nullptr || (road->flags & kEdgeRoadFlag) == 0 ||
        (road->flags & (kEdgeRiverFlag | kEdgeBridgeFlag)) != 0 ||
        road->move_cost <= standard_road->move_cost) {
        throw std::runtime_error{
            "civilization.toml history road_edge 必須是純道路且移動成本高於 edge.road"};
    }

    const auto* ruins = (*history_table)["ruin_features"].as_array();
    if (ruins == nullptr || ruins->size() != history.ruin_features.size()) {
        throw std::runtime_error{"civilization.toml history ruin_features 必須有三級"};
    }
    std::set<std::uint16_t> ruin_ids;
    for (std::size_t index = 0; index < history.ruin_features.size(); ++index) {
        const auto string_id = (*ruins)[index].value<std::string>();
        const auto feature =
            string_id.has_value() ? result.find_feature(*string_id) : std::nullopt;
        const auto* definition = feature.has_value() ? result.feature(*feature) : nullptr;
        if (definition == nullptr || (definition->flags & kFeatureRuinFlag) == 0) {
            throw std::runtime_error{
                "civilization.toml history ruin_features 引用不存在或不是廢墟 def"};
        }
        if (!ruin_ids.insert(value_of(*feature)).second) {
            throw std::runtime_error{
                "civilization.toml history ruin_features 三級 ID 不得重複"};
        }
        history.ruin_features[index] = *feature;
    }
}

}  // namespace aetheria::rules

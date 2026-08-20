// core/rules/ruleset_load_civilization.cpp：城市評分／道路工程規則的載入。

#include "core/rules/ruleset.h"
#include "core/rules/toml_read.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>

namespace aetheria::rules {

using namespace detail;

void RulesetLoader::load_civilization_rules(Ruleset& result,
                                            const std::filesystem::path& data_directory) {
    const auto civilization_path = data_directory / "civilization.toml";
    if (std::filesystem::is_regular_file(civilization_path)) {
        toml::table civilization;
        try {
            civilization = toml::parse_file(civilization_path.string());
        } catch (const toml::parse_error& error) {
            throw std::runtime_error{"Ruleset TOML 格式錯誤：" + civilization_path.string() +
                                     "：" + std::string{error.description()}};
        }
        auto& rules = result.civilization_rules_;
        auto& weights = rules.scoring_weights;
        weights.freshwater = require_int32(civilization, "freshwater_weight", civilization_path);
        weights.farmland = require_int32(civilization, "farmland_weight", civilization_path);
        weights.harbor = require_int32(civilization, "harbor_weight", civilization_path);
        weights.defense = require_int32(civilization, "defense_weight", civilization_path);
        weights.resource = require_int32(civilization, "resource_weight", civilization_path);
        weights.bottleneck = require_int32(civilization, "bottleneck_weight", civilization_path);
        weights.extreme_climate_penalty =
            require_int32(civilization, "extreme_climate_penalty", civilization_path);
        weights.high_elevation_penalty =
            require_int32(civilization, "high_elevation_penalty", civilization_path);
        auto positive_u16 = [&](std::string_view field) {
            const auto value = require_integer(civilization, field, civilization_path);
            if (value <= 0 || value > UINT16_MAX) {
                throw std::runtime_error{"civilization.toml 欄位必須是正 uint16：" +
                                         std::string{field}};
            }
            return static_cast<std::uint16_t>(value);
        };
        rules.high_elevation_threshold = positive_u16("high_elevation_threshold");
        rules.target_city_count = positive_u16("target_city_count");
        rules.town_count = positive_u16("town_count");
        rules.major_city_count = static_cast<std::uint16_t>(rules.factions.faction_count * 2U);
        const auto bottleneck_radius = positive_u16("bottleneck_radius");
        const auto loop_percent = positive_u16("loop_percent");
        if (bottleneck_radius > 8 || loop_percent > UINT8_MAX) {
            throw std::runtime_error{"civilization.toml 半徑或環路比例超出 uint8"};
        }
        rules.bottleneck_radius = static_cast<std::uint8_t>(bottleneck_radius);
        rules.loop_percent = static_cast<std::uint8_t>(loop_percent);
        rules.road_base_cost = positive_u16("road_base_cost");
        rules.road_terrain_weight = positive_u16("road_terrain_weight");
        rules.road_slope_weight = positive_u16("road_slope_weight");
        rules.road_slope_divisor = positive_u16("road_slope_divisor");
        rules.road_valley_discount = positive_u16("road_valley_discount");
        rules.road_swamp_penalty = positive_u16("road_swamp_penalty");
        rules.road_river_crossing_penalty = positive_u16("road_river_crossing_penalty");
        rules.road_reuse_numerator = positive_u16("road_reuse_numerator");
        rules.road_reuse_denominator = positive_u16("road_reuse_denominator");
        if (rules.major_city_count + rules.town_count > rules.target_city_count ||
            rules.loop_percent < 10 || rules.loop_percent > 20 ||
            rules.road_reuse_numerator >= rules.road_reuse_denominator) {
            throw std::runtime_error{"civilization.toml 的數量、環路或道路折扣無效"};
        }
        auto read_u16_array = [&](std::string_view field, auto& target) {
            const auto* values = civilization[field].as_array();
            if (values == nullptr || values->size() != target.size()) {
                throw std::runtime_error{"civilization.toml 陣列尺寸無效：" +
                                         std::string{field}};
            }
            for (std::size_t index = 0; index < target.size(); ++index) {
                const auto value = (*values)[index].value<std::int64_t>();
                if (!value.has_value() || *value <= 0 || *value > UINT16_MAX) {
                    throw std::runtime_error{"civilization.toml 陣列值無效：" +
                                             std::string{field}};
                }
                target[index] = static_cast<std::uint16_t>(*value);
            }
        };
        read_u16_array("minimum_spacing", rules.minimum_spacing);
        read_u16_array("road_usage_thresholds", rules.road_usage_thresholds);
        if (!std::is_sorted(rules.minimum_spacing.begin(), rules.minimum_spacing.end()) ||
            !std::is_sorted(rules.road_usage_thresholds.begin(),
                            rules.road_usage_thresholds.end())) {
            throw std::runtime_error{"civilization.toml 間距與道路門檻必須遞增"};
        }
        const auto swamp_id = require_string(civilization, "swamp_terrain", civilization_path);
        const auto swamp = result.find_terrain(swamp_id);
        if (!swamp.has_value()) {
            throw std::runtime_error{"civilization.toml 引用不存在的 swamp terrain：" + swamp_id};
        }
        rules.swamp_terrain = *swamp;
        const auto* road_edges = civilization["road_edges"].as_array();
        if (road_edges == nullptr || road_edges->size() != rules.road_edges.size()) {
            throw std::runtime_error{"civilization.toml road_edges 必須有三級"};
        }
        for (std::size_t index = 0; index < rules.road_edges.size(); ++index) {
            const auto string_id = (*road_edges)[index].value<std::string>();
            const auto edge = string_id.has_value() ? result.find_edge(*string_id) : std::nullopt;
            if (!edge.has_value()) {
                throw std::runtime_error{"civilization.toml road_edges 引用不存在"};
            }
            rules.road_edges[index] = *edge;
        }
        const auto* crossings = civilization["crossings"].as_array();
        if (crossings == nullptr || crossings->empty()) {
            throw std::runtime_error{"civilization.toml 缺少 [[crossings]]"};
        }
        for (const auto& node : *crossings) {
            const auto& table = require_table(node, civilization_path);
            const auto river_string = require_string(table, "river", civilization_path);
            const auto road_string = require_string(table, "road", civilization_path);
            const auto result_string = require_string(table, "result", civilization_path);
            const auto river = result.find_edge(river_string);
            const auto road = result.find_edge(road_string);
            const auto compound = result.find_edge(result_string);
            if (!river.has_value() || !road.has_value() || !compound.has_value()) {
                throw std::runtime_error{"civilization.toml crossing 引用不存在的 edge"};
            }
            rules.crossings.push_back({*river, *road, *compound});
        }
        load_crossing_rules(result, rules);
    }
}

}  // namespace aetheria::rules

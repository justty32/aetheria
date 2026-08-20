// core/rules/ruleset_load_history_values.cpp：上古歷史數值與結構限制的載入。

#include "core/rules/ruleset_load_history_detail.h"
#include "core/rules/toml_read.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>

namespace aetheria::rules::detail {
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

}  // namespace

void load_history_values(CivilizationRules::HistoryRules& history,
                         const toml::table& history_table,
                         const std::filesystem::path& civilization_path,
                         const CivilizationRules& civilization_rules) {
    auto& weights = history.scoring_weights;
    weights.freshwater = require_int32(history_table, "freshwater_weight", civilization_path);
    weights.farmland = require_int32(history_table, "farmland_weight", civilization_path);
    weights.harbor = require_int32(history_table, "harbor_weight", civilization_path);
    weights.defense = require_int32(history_table, "defense_weight", civilization_path);
    weights.resource = require_int32(history_table, "resource_weight", civilization_path);
    weights.bottleneck = require_int32(history_table, "bottleneck_weight", civilization_path);
    weights.extreme_climate_penalty =
        require_int32(history_table, "extreme_climate_penalty", civilization_path);
    weights.high_elevation_penalty =
        require_int32(history_table, "high_elevation_penalty", civilization_path);
    history.ancient_site_count =
        require_history_u16(history_table, "ancient_site_count", civilization_path, true);
    history.ancient_city_count =
        require_history_u16(history_table, "ancient_city_count", civilization_path, true);
    history.ancient_town_count =
        require_history_u16(history_table, "ancient_town_count", civilization_path, true);
    history.ancient_road_reuse_numerator = require_history_u16(
        history_table, "ancient_road_reuse_numerator", civilization_path, false);
    history.ancient_road_reuse_denominator = require_history_u16(
        history_table, "ancient_road_reuse_denominator", civilization_path, false);

    const auto survivor_percent =
        require_history_u16(history_table, "survivor_percent", civilization_path, false);
    if (survivor_percent > 100) {
        throw std::runtime_error{"civilization.toml history survivor_percent 必須介於 1 與 100"};
    }
    history.survivor_percent = static_cast<std::uint8_t>(survivor_percent);

    history.ancient_site_bonus =
        require_int32(history_table, "ancient_site_bonus", civilization_path);
    if (history.ancient_site_bonus < 0) {
        throw std::runtime_error{"civilization.toml history ancient_site_bonus 不得為負"};
    }

    const auto* spacing = history_table["minimum_spacing"].as_array();
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
}

}  // namespace aetheria::rules::detail

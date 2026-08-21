// ruleset_load_site_build.cpp：城建建築、相鄰效果與人口成長規則載入。

#include "core/rules/ruleset.h"

#include "core/rules/toml_read.h"

#include <toml++/toml.hpp>

#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace aetheria::rules {

using namespace detail;

void RulesetLoader::load_site_build(Ruleset& result,
                                    const std::filesystem::path& data_directory,
                                    std::set<std::string, std::less<>>& global_ids) {
    const auto path = data_directory / "site_build.toml";
    toml::table document;
    try {
        document = toml::parse_file(path.string());
    } catch (const toml::parse_error& error) {
        throw std::runtime_error{"Ruleset TOML 格式錯誤：" + path.string() + "：" +
                                 std::string{error.description()}};
    }
    const auto* growth = document["growth"].as_table();
    const auto* buildings = document["buildings"].as_array();
    const auto* adjacency = document["adjacency"].as_array();
    if (growth == nullptr || buildings == nullptr || buildings->empty() || adjacency == nullptr) {
        throw std::runtime_error{"site_build.toml 缺少 growth、buildings 或 adjacency"};
    }

    const auto read_positive = [&](const toml::table& table, std::string_view field,
                                   std::int64_t maximum) {
        const auto value = require_integer(table, field, path);
        if (value <= 0 || value > maximum) {
            throw std::runtime_error{"site_build.toml 正整數無效：" + std::string{field}};
        }
        return value;
    };
    auto& rules = result.site_build_rules_;
    rules.base_growth_basis_points_per_xun = static_cast<std::uint16_t>(
        read_positive(*growth, "base_growth_basis_points_per_xun", 10'000));
    rules.people_supported_per_food = static_cast<std::uint16_t>(
        read_positive(*growth, "people_supported_per_food", UINT16_MAX));
    const auto base_satisfaction = require_integer(*growth, "base_satisfaction", path);
    if (base_satisfaction < 0 || base_satisfaction > 100) {
        throw std::runtime_error{"site_build.toml base_satisfaction 必須為 0～100"};
    }
    rules.base_satisfaction = static_cast<std::uint8_t>(base_satisfaction);

    for (const auto& node : *buildings) {
        const auto& table = require_table(node, path);
        CityBuildingDef def;
        def.id = require_string(table, "id", path);
        register_global_id(global_ids, def.id, "city.");
        def.width = static_cast<std::uint8_t>(read_positive(table, "width", 4));
        def.height = static_cast<std::uint8_t>(read_positive(table, "height", 4));
        def.construction_hours = static_cast<std::uint16_t>(
            read_positive(table, "construction_hours", UINT16_MAX));
        const auto read_nonnegative = [&](std::string_view field, std::int64_t maximum) {
            const auto value = require_integer(table, field, path);
            if (value < 0 || value > maximum) {
                throw std::runtime_error{"site_build.toml 建築數值無效：" + def.id + "." +
                                         std::string{field}};
            }
            return value;
        };
        def.housing_capacity = static_cast<std::uint32_t>(
            read_nonnegative("housing_capacity", UINT32_MAX));
        def.food_per_hour =
            static_cast<std::uint16_t>(read_nonnegative("food_per_hour", UINT16_MAX));
        def.production_per_hour =
            static_cast<std::uint16_t>(read_nonnegative("production_per_hour", UINT16_MAX));
        def.satisfaction =
            static_cast<std::int16_t>(read_nonnegative("satisfaction", INT16_MAX));
        const auto id = append_def<CityBuildingDefId>(result.city_buildings_, std::move(def));
        result.city_building_index_.emplace(result.city_buildings_.back().id, id);
    }

    for (const auto& node : *adjacency) {
        const auto& table = require_table(node, path);
        const auto source_name = require_string(table, "source", path);
        const auto neighbor_name = require_string(table, "neighbor", path);
        const auto source = result.find_city_building(source_name);
        const auto neighbor = result.find_city_building(neighbor_name);
        const auto production = require_integer(table, "production_per_hour", path);
        const auto satisfaction = require_integer(table, "satisfaction", path);
        if (!source.has_value() || !neighbor.has_value() || production < INT16_MIN ||
            production > INT16_MAX || satisfaction < INT16_MIN || satisfaction > INT16_MAX ||
            (production == 0 && satisfaction == 0)) {
            throw std::runtime_error{"site_build.toml 相鄰效果無效：" + source_name + " → " +
                                     neighbor_name};
        }
        result.city_buildings_.at(value_of(*source)).adjacency.push_back(
            {*neighbor, static_cast<std::int16_t>(production),
             static_cast<std::int16_t>(satisfaction)});
    }
    rules.loaded = true;
}

}  // namespace aetheria::rules

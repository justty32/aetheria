// core/rules/ruleset_load_biomes.cpp：terrain／relief 規則表與四季移動倍率的載入。

#include "core/rules/ruleset.h"
#include "core/rules/toml_read.h"

#include <toml++/toml.hpp>

#include <limits>
#include <stdexcept>
#include <string>

namespace aetheria::rules {

using namespace detail;
namespace {

template <typename Value>
[[nodiscard]] Value optional_bounded_integer(const toml::table& table, std::string_view field,
                                             Value fallback, const std::filesystem::path& path) {
    const auto value = table[field].value<std::int64_t>();
    if (!value.has_value()) {
        return fallback;
    }
    if (*value < static_cast<std::int64_t>(std::numeric_limits<Value>::min()) ||
        *value > static_cast<std::int64_t>(std::numeric_limits<Value>::max())) {
        throw std::runtime_error{"Ruleset 整數欄位超出範圍 " + std::string{field} + "：" +
                                 path.string()};
    }
    return static_cast<Value>(*value);
}

}  // namespace

void RulesetLoader::load_biome_rule_tables(Ruleset& result,
                                           const std::filesystem::path& data_directory) {
    const auto biome_path = data_directory / "biomes.toml";
    if (std::filesystem::is_regular_file(biome_path)) {
        bool saw_terrain_fallback{};
        for (const auto& node : read_array(biome_path, "terrain_rules")) {
            const auto& table = require_table(node, biome_path);
            if (saw_terrain_fallback) {
                throw std::runtime_error{"Terrain fallback 後不得再有規則：" +
                                         biome_path.string()};
            }
            TerrainRule rule;
            rule.fallback = table["fallback"].value_or(false);
            rule.min_temperature_tenths = optional_bounded_integer<std::int16_t>(
                table, "min_temperature_tenths", rule.min_temperature_tenths, biome_path);
            rule.max_temperature_tenths = optional_bounded_integer<std::int16_t>(
                table, "max_temperature_tenths", rule.max_temperature_tenths, biome_path);
            rule.min_moisture = optional_bounded_integer<std::uint16_t>(
                table, "min_moisture", rule.min_moisture, biome_path);
            rule.max_moisture = optional_bounded_integer<std::uint16_t>(
                table, "max_moisture", rule.max_moisture, biome_path);
            rule.min_elevation = optional_bounded_integer<std::uint16_t>(
                table, "min_elevation", rule.min_elevation, biome_path);
            rule.max_elevation = optional_bounded_integer<std::uint16_t>(
                table, "max_elevation", rule.max_elevation, biome_path);
            if (rule.min_temperature_tenths > rule.max_temperature_tenths ||
                rule.min_moisture > rule.max_moisture ||
                rule.min_elevation > rule.max_elevation) {
                throw std::runtime_error{"TerrainRule 範圍上下界顛倒：" +
                                         biome_path.string()};
            }
            const auto terrain_string = require_string(table, "terrain", biome_path);
            const auto terrain = result.find_terrain(terrain_string);
            if (!terrain.has_value()) {
                throw std::runtime_error{"TerrainRule 引用不存在的 def：" + terrain_string};
            }
            rule.terrain = *terrain;
            saw_terrain_fallback = rule.fallback;
            result.terrain_rules_.push_back(rule);
        }
        if (result.terrain_rules_.empty() || !saw_terrain_fallback) {
            throw std::runtime_error{"TerrainRule 最後一條必須是 fallback：" +
                                     biome_path.string()};
        }

        bool saw_relief_fallback{};
        for (const auto& node : read_array(biome_path, "relief_rules")) {
            const auto& table = require_table(node, biome_path);
            if (saw_relief_fallback) {
                throw std::runtime_error{"Relief fallback 後不得再有規則：" +
                                         biome_path.string()};
            }
            ReliefRule rule;
            rule.fallback = table["fallback"].value_or(false);
            rule.min_elevation = optional_bounded_integer<std::uint16_t>(
                table, "min_elevation", rule.min_elevation, biome_path);
            rule.max_elevation = optional_bounded_integer<std::uint16_t>(
                table, "max_elevation", rule.max_elevation, biome_path);
            rule.min_ruggedness = optional_bounded_integer<std::uint16_t>(
                table, "min_ruggedness", rule.min_ruggedness, biome_path);
            rule.max_ruggedness = optional_bounded_integer<std::uint16_t>(
                table, "max_ruggedness", rule.max_ruggedness, biome_path);
            if (rule.min_elevation > rule.max_elevation ||
                rule.min_ruggedness > rule.max_ruggedness) {
                throw std::runtime_error{"ReliefRule 範圍上下界顛倒：" +
                                         biome_path.string()};
            }
            const auto relief_string = require_string(table, "relief", biome_path);
            const auto relief = result.find_relief(relief_string);
            if (!relief.has_value()) {
                throw std::runtime_error{"ReliefRule 引用不存在的 def：" + relief_string};
            }
            rule.relief = *relief;
            saw_relief_fallback = rule.fallback;
            result.relief_rules_.push_back(rule);
        }
        if (result.relief_rules_.empty() || !saw_relief_fallback) {
            throw std::runtime_error{"ReliefRule 最後一條必須是 fallback：" +
                                     biome_path.string()};
        }
    }
}

void RulesetLoader::load_movement_rules(Ruleset& result,
                                        const std::filesystem::path& data_directory) {
    const auto movement_path = data_directory / "movement.toml";
    if (std::filesystem::is_regular_file(movement_path)) {
        toml::table movement;
        try {
            movement = toml::parse_file(movement_path.string());
        } catch (const toml::parse_error& error) {
            throw std::runtime_error{"Ruleset TOML 格式錯誤：" + movement_path.string() + "：" +
                                     std::string{error.description()}};
        }
        const auto* numerators = movement["season_numerators"].as_array();
        const auto denominator = movement["season_denominator"].value<std::int64_t>();
        if (numerators == nullptr || numerators->size() != 4 || !denominator.has_value() ||
            *denominator <= 0 || *denominator > UINT16_MAX) {
            throw std::runtime_error{"movement.toml 的四季倍率格式無效"};
        }
        for (std::size_t index = 0; index < numerators->size(); ++index) {
            const auto numerator = (*numerators)[index].value<std::int64_t>();
            if (!numerator.has_value() || *numerator <= 0 || *numerator > UINT16_MAX) {
                throw std::runtime_error{"movement.toml 的四季倍率必須是正整數"};
            }
            result.movement_rules_.season_numerators[index] =
                static_cast<std::uint16_t>(*numerator);
        }
        result.movement_rules_.season_denominator = static_cast<std::uint16_t>(*denominator);
        result.movement_rules_.loaded = true;
    }
}

}  // namespace aetheria::rules

// ruleset_load_world_observations.cpp：世界觀測初值與任務門檻載入。

#include "core/rules/ruleset.h"

#include "core/rules/toml_read.h"

#include <toml++/toml.hpp>

#include <limits>
#include <stdexcept>
#include <string>

namespace aetheria::rules {

using namespace detail;

void RulesetLoader::load_world_observation_rules(Ruleset& result,
                                                 const std::filesystem::path& data_directory) {
    const auto path = data_directory / "world_observations.toml";
    toml::table document;
    try {
        document = toml::parse_file(path.string());
    } catch (const toml::parse_error& error) {
        throw std::runtime_error{"Ruleset TOML 格式錯誤：" + path.string() + "：" +
                                 std::string{error.description()}};
    }
    const auto* order = document["order"].as_table();
    const auto* quests = document["quests"].as_table();
    if (order == nullptr || quests == nullptr) {
        throw std::runtime_error{"world_observations.toml 缺少 order 或 quests"};
    }

    const auto read_u16 = [&](const toml::table& table, std::string_view field,
                              bool allow_zero = true) {
        const auto value = require_integer(table, field, path);
        if (value < (allow_zero ? 0 : 1) || value > UINT16_MAX) {
            throw std::runtime_error{"world_observations.toml 整數無效：" + std::string{field}};
        }
        return static_cast<std::uint16_t>(value);
    };
    auto& rules = result.world_observation_rules_;
    rules.initial_garrison_coverage = read_u16(*order, "initial_garrison_coverage");
    rules.initial_patrol_coverage = read_u16(*order, "initial_patrol_coverage");
    rules.initial_bandit_pressure = read_u16(*order, "initial_bandit_pressure");
    rules.initial_refugee_pressure = read_u16(*order, "initial_refugee_pressure");
    rules.bandit_minimum_order = read_u16(*quests, "bandit_minimum_order", false);
    rules.bandit_pressure_reduction = read_u16(*quests, "bandit_pressure_reduction", false);
    const auto food_required = require_integer(*quests, "food_delivery_required", path);
    if (food_required <= 0) {
        throw std::runtime_error{"world_observations.toml food_delivery_required 必須為正數"};
    }
    rules.food_delivery_required = static_cast<std::uint64_t>(food_required);
    rules.dungeon_minimum_depth = read_u16(*quests, "dungeon_minimum_depth", false);

    const auto coverage =
        static_cast<std::uint32_t>(rules.initial_garrison_coverage) + rules.initial_patrol_coverage;
    const auto pressure =
        static_cast<std::uint32_t>(rules.initial_bandit_pressure) + rules.initial_refugee_pressure;
    if (coverage > UINT16_MAX || pressure > UINT16_MAX ||
        rules.bandit_pressure_reduction > rules.initial_bandit_pressure) {
        throw std::runtime_error{"world_observations.toml 治安總量或清剿減壓量無效"};
    }
    rules.loaded = true;
}

}  // namespace aetheria::rules

// core/rules/ruleset_load_site_wilderness.cpp：荒野 W1～W6 的有界成本與密度參數。

#include "core/rules/ruleset.h"

#include "core/rules/toml_read.h"

#include <toml++/toml.hpp>

#include <limits>
#include <stdexcept>
#include <string>

namespace aetheria::rules {

void RulesetLoader::load_site_wilderness(Ruleset& result,
                                         const std::filesystem::path& data_directory) {
    const auto path = data_directory / "site_wild.toml";
    toml::table document;
    try {
        document = toml::parse_file(path.string());
    } catch (const toml::parse_error& error) {
        throw std::runtime_error{"Ruleset TOML 格式錯誤：" + path.string() + "：" +
                                 std::string{error.description()}};
    }
    const auto* table = document["wilderness"].as_table();
    if (table == nullptr) {
        throw std::runtime_error{"site_wild.toml 缺少 wilderness 區段"};
    }
    auto read = [&](std::string_view field, std::int64_t minimum,
                    std::int64_t maximum) -> std::int64_t {
        const auto value = detail::require_integer(*table, field, path);
        if (value < minimum || value > maximum) {
            throw std::runtime_error{"site_wild.toml 參數無效：" + std::string{field}};
        }
        return value;
    };

    auto& rules = result.wilderness_generation_rules_;
    rules.height_noise_amplitude = static_cast<std::uint16_t>(
        read("height_noise_amplitude", 1, std::numeric_limits<std::uint16_t>::max()));
    rules.plain_passable_slope = static_cast<std::uint16_t>(
        read("plain_passable_slope", 1, std::numeric_limits<std::uint16_t>::max()));
    rules.hills_passable_slope = static_cast<std::uint16_t>(
        read("hills_passable_slope", 1, std::numeric_limits<std::uint16_t>::max()));
    rules.mountain_passable_slope = static_cast<std::uint16_t>(
        read("mountain_passable_slope", 1, std::numeric_limits<std::uint16_t>::max()));
    rules.jitter_cell_extent = static_cast<std::uint8_t>(read("jitter_cell_extent", 2, 16));
    rules.sparse_vegetation_percent =
        static_cast<std::uint8_t>(read("sparse_vegetation_percent", 0, 100));
    rules.forest_vegetation_percent =
        static_cast<std::uint8_t>(read("forest_vegetation_percent", 0, 100));
    rules.base_resource_points =
        static_cast<std::uint8_t>(read("base_resource_points", 0, 64));
    rules.mine_resource_points =
        static_cast<std::uint8_t>(read("mine_resource_points", 0, 64));
    rules.owned_encounter_points =
        static_cast<std::uint8_t>(read("owned_encounter_points", 0, 64));
    rules.unowned_encounter_points =
        static_cast<std::uint8_t>(read("unowned_encounter_points", 0, 64));
    rules.road_traveler_points =
        static_cast<std::uint8_t>(read("road_traveler_points", 0, 64));
    rules.wilderness_portals =
        static_cast<std::uint8_t>(read("wilderness_portals", 0, 16));
    rules.mountain_portals =
        static_cast<std::uint8_t>(read("mountain_portals", 0, 16));
    rules.ruin_portals = static_cast<std::uint8_t>(read("ruin_portals", 0, 16));
    rules.ruin_keep_min_percent =
        static_cast<std::uint8_t>(read("ruin_keep_min_percent", 0, 100));
    rules.ruin_keep_max_percent =
        static_cast<std::uint8_t>(read("ruin_keep_max_percent", 0, 100));
    if (rules.ruin_keep_min_percent > rules.ruin_keep_max_percent) {
        throw std::runtime_error{"site_wild.toml 廢墟保留比例上下限顛倒"};
    }
    rules.loaded = true;
}

}  // namespace aetheria::rules

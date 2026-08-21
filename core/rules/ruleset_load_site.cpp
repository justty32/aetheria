// core/rules/ruleset_load_site.cpp：Site ground def 與 Terrain→Ground 投影表載入。

#include "core/rules/ruleset.h"

#include "core/rules/toml_read.h"

#include <toml++/toml.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace aetheria::rules {

using namespace detail;

void RulesetLoader::load_grounds(Ruleset& result, const std::filesystem::path& data_directory,
                                 std::set<std::string, std::less<>>& global_ids) {
    const auto ground_path = data_directory / "ground.toml";
    for (const auto& node : read_defs(ground_path)) {
        const auto& table = require_table(node, ground_path);
        GroundDef def;
        read_common(table, ground_path, def, true);
        register_global_id(global_ids, def.id, "ground.");
        const auto id = append_def<GroundId>(result.grounds_, std::move(def));
        result.ground_index_.emplace(result.grounds_.back().id, id);
    }
}

void RulesetLoader::load_site_projection(Ruleset& result,
                                         const std::filesystem::path& data_directory) {
    const auto projection_path = data_directory / "site_projection.toml";
    toml::table document;
    try {
        document = toml::parse_file(projection_path.string());
    } catch (const toml::parse_error& error) {
        throw std::runtime_error{"Ruleset TOML 格式錯誤：" + projection_path.string() + "：" +
                                 std::string{error.description()}};
    }
    const auto* city = document["city_skeleton"].as_table();
    const auto* mappings = document["terrain_ground"].as_array();
    if (city == nullptr || mappings == nullptr) {
        throw std::runtime_error{"site_projection.toml 缺少 city_skeleton 或 terrain_ground"};
    }
    auto read_positive = [&](std::string_view field, std::int64_t maximum) {
        const auto value = require_integer(*city, field, projection_path);
        if (value <= 0 || value > maximum) {
            throw std::runtime_error{"site_projection.toml 城區骨架參數無效：" +
                                     std::string{field}};
        }
        return value;
    };
    auto& site = result.site_generation_rules_;
    site.block_split_depth =
        static_cast<std::uint8_t>(read_positive("block_split_depth", 8));
    site.block_cut_min_percent =
        static_cast<std::uint8_t>(read_positive("block_cut_min_percent", 49));
    site.block_cut_max_percent =
        static_cast<std::uint8_t>(read_positive("block_cut_max_percent", 49));
    site.block_min_extent =
        static_cast<std::uint8_t>(read_positive("block_min_extent", 16));
    site.height_noise_amplitude =
        static_cast<std::uint16_t>(read_positive("height_noise_amplitude", UINT16_MAX));
    site.max_buildable_slope =
        static_cast<std::uint16_t>(read_positive("max_buildable_slope", UINT16_MAX));
    site.water_inland_reach =
        static_cast<std::uint8_t>(read_positive("water_inland_reach", 63));
    if (site.block_cut_min_percent > site.block_cut_max_percent ||
        site.block_min_extent * 2U + 1U >= 64U) {
        throw std::runtime_error{"site_projection.toml 街廓切分範圍無效"};
    }
    site.loaded = true;

    std::vector<std::optional<TerrainGroundMapping>> by_terrain(result.terrains_.size());
    const auto load_mappings = [&](const toml::array& entries,
                                   const std::filesystem::path& path) {
        for (const auto& node : entries) {
            const auto& table = require_table(node, path);
            const auto terrain_string = require_string(table, "terrain", path);
            const auto ground_string = require_string(table, "ground", path);
            const auto rough_ground_string = require_string(table, "rough_ground", path);
            const auto terrain = result.find_terrain(terrain_string);
            const auto ground = result.find_ground(ground_string);
            const auto rough_ground = result.find_ground(rough_ground_string);
            if (!terrain.has_value()) {
                throw std::runtime_error{"Site ground 映射引用不存在的 TerrainDef：" +
                                         terrain_string};
            }
            if (!ground.has_value()) {
                throw std::runtime_error{"Site ground 映射引用不存在的 GroundDef：" +
                                         ground_string};
            }
            if (!rough_ground.has_value()) {
                throw std::runtime_error{"Site ground 映射引用不存在的 GroundDef：" +
                                         rough_ground_string};
            }
            auto& slot = by_terrain.at(value_of(*terrain));
            if (slot.has_value()) {
                throw std::runtime_error{"TerrainDef 有重複 Site ground 映射：" +
                                         terrain_string};
            }
            slot = TerrainGroundMapping{*terrain, *ground, *rough_ground};
        }
    };
    load_mappings(*mappings, projection_path);
    const auto biome_path = data_directory / "biomes.toml";
    if (std::filesystem::is_regular_file(biome_path)) {
        load_mappings(read_array(biome_path, "terrain_ground"), biome_path);
    }

    result.terrain_ground_mappings_.reserve(by_terrain.size());
    for (std::size_t index = 0; index < by_terrain.size(); ++index) {
        if (!by_terrain[index].has_value()) {
            throw std::runtime_error{"TerrainDef 缺少 Site ground 映射：" +
                                     result.terrains_.at(index).id};
        }
        result.terrain_ground_mappings_.push_back(*by_terrain[index]);
    }
}

}  // namespace aetheria::rules

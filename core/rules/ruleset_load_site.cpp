// core/rules/ruleset_load_site.cpp：Site ground def 與 Terrain→Ground 投影表載入。

#include "core/rules/ruleset.h"

#include "core/rules/toml_read.h"

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
    std::vector<std::optional<TerrainGroundMapping>> by_terrain(result.terrains_.size());
    for (const auto& node : read_array(projection_path, "terrain_ground")) {
        const auto& table = require_table(node, projection_path);
        const auto terrain_string = require_string(table, "terrain", projection_path);
        const auto ground_string = require_string(table, "ground", projection_path);
        const auto rough_ground_string = require_string(table, "rough_ground", projection_path);
        const auto terrain = result.find_terrain(terrain_string);
        const auto ground = result.find_ground(ground_string);
        const auto rough_ground = result.find_ground(rough_ground_string);
        if (!terrain.has_value()) {
            throw std::runtime_error{"Site ground 映射引用不存在的 TerrainDef：" + terrain_string};
        }
        if (!ground.has_value()) {
            throw std::runtime_error{"Site ground 映射引用不存在的 GroundDef：" + ground_string};
        }
        if (!rough_ground.has_value()) {
            throw std::runtime_error{"Site ground 映射引用不存在的 GroundDef：" +
                                     rough_ground_string};
        }
        auto& slot = by_terrain.at(value_of(*terrain));
        if (slot.has_value()) {
            throw std::runtime_error{"TerrainDef 有重複 Site ground 映射：" + terrain_string};
        }
        slot = TerrainGroundMapping{*terrain, *ground, *rough_ground};
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

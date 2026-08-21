// core/rules/ruleset_load_defs.cpp：terrain／relief／feature／edge 四類基礎 def 的載入。

#include "core/rules/ruleset.h"
#include "core/rules/toml_read.h"

#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace aetheria::rules {

using namespace detail;

void RulesetLoader::load_terrains(Ruleset& result, const std::filesystem::path& data_directory,
                                  std::set<std::string, std::less<>>& global_ids) {
    const auto terrain_path = data_directory / "terrain.toml";
    const auto biome_path = data_directory / "biomes.toml";
    const auto load_from = [&](const std::filesystem::path& path, std::string_view section) {
        for (const auto& node : read_array(path, section)) {
            const auto& table = require_table(node, path);
            TerrainDef def;
            read_common(table, path, def);
            const auto* yield = table["yield"].as_table();
            if (yield == nullptr) {
                throw std::runtime_error{"TerrainDef 缺少 yield 區段：" + def.id};
            }
            def.yield.food = require_int32(*yield, "food", path);
            def.yield.production = require_int32(*yield, "production", path);
            def.yield.wealth = require_int32(*yield, "wealth", path);
            def.yield.mana = require_int32(*yield, "mana", path);
            register_global_id(global_ids, def.id, "terrain.");
            const auto id = append_def<TerrainId>(result.terrains_, std::move(def));
            result.terrain_index_.emplace(result.terrains_.back().id, id);
        }
    };
    load_from(terrain_path, "defs");
    if (std::filesystem::is_regular_file(biome_path)) {
        load_from(biome_path, "terrain_defs");
    }
}

void RulesetLoader::load_reliefs(Ruleset& result, const std::filesystem::path& data_directory,
                                 std::set<std::string, std::less<>>& global_ids) {
    const auto relief_path = data_directory / "relief.toml";
    for (const auto& node : read_defs(relief_path)) {
        const auto& table = require_table(node, relief_path);
        ReliefDef def;
        read_common(table, relief_path, def);
        register_global_id(global_ids, def.id, "relief.");
        const auto id = append_def<ReliefId>(result.reliefs_, std::move(def));
        result.relief_index_.emplace(result.reliefs_.back().id, id);
    }
}

void RulesetLoader::load_features(
    Ruleset& result, const std::filesystem::path& data_directory,
    std::set<std::string, std::less<>>& global_ids,
    std::vector<std::pair<std::size_t, std::string>>& feature_terrain_references) {
    const auto feature_path = data_directory / "feature.toml";
    for (const auto& node : read_defs(feature_path)) {
        const auto& table = require_table(node, feature_path);
        FeatureDef def;
        read_common(table, feature_path, def, true);
        const auto reference = table["required_terrain"].value<std::string>();
        register_global_id(global_ids, def.id, "feature.");
        const auto id = append_def<FeatureId>(result.features_, std::move(def));
        result.feature_index_.emplace(result.features_.back().id, id);
        if (reference.has_value()) {
            feature_terrain_references.emplace_back(value_of(id), *reference);
        }
    }
}

void RulesetLoader::load_edges(
    Ruleset& result, const std::filesystem::path& data_directory,
    std::set<std::string, std::less<>>& global_ids,
    std::vector<std::pair<std::size_t, std::string>>& feature_terrain_references) {
    const auto edge_path = data_directory / "edges.toml";
    for (const auto& node : read_defs(edge_path)) {
        const auto& table = require_table(node, edge_path);
        EdgeDef def;
        read_common(table, edge_path, def, true);
        register_global_id(global_ids, def.id, "edge.");
        const auto id = append_def<EdgeId>(result.edges_, std::move(def));
        result.edge_index_.emplace(result.edges_.back().id, id);
    }

    for (const auto& [feature_index, terrain_string_id] : feature_terrain_references) {
        const auto terrain = result.find_terrain(terrain_string_id);
        if (!terrain.has_value()) {
            throw std::runtime_error{"FeatureDef 引用不存在的 terrain id：" + terrain_string_id};
        }
        result.features_.at(feature_index).required_terrain = *terrain;
    }
}

}  // namespace aetheria::rules

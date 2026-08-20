#include "core/rules/ruleset.h"

#include "core/rules/toml_read.h"

#include <set>
#include <utility>

namespace aetheria::rules {

using namespace detail;

const TerrainDef* Ruleset::terrain(TerrainId id) const noexcept { return lookup(terrains(), id); }
const ReliefDef* Ruleset::relief(ReliefId id) const noexcept { return lookup(reliefs(), id); }
const FeatureDef* Ruleset::feature(FeatureId id) const noexcept { return lookup(features(), id); }
const EdgeDef* Ruleset::edge(EdgeId id) const noexcept { return lookup(edges(), id); }

std::optional<TerrainId> Ruleset::find_terrain(std::string_view id) const noexcept {
    return find_id(terrain_index_, id);
}
std::optional<ReliefId> Ruleset::find_relief(std::string_view id) const noexcept {
    return find_id(relief_index_, id);
}
std::optional<FeatureId> Ruleset::find_feature(std::string_view id) const noexcept {
    return find_id(feature_index_, id);
}
std::optional<EdgeId> Ruleset::find_edge(std::string_view id) const noexcept {
    return find_id(edge_index_, id);
}

Ruleset RulesetLoader::load(const std::filesystem::path& data_directory) {
    Ruleset result;
    std::set<std::string, std::less<>> global_ids;
    std::vector<std::pair<std::size_t, std::string>> feature_terrain_references;

    load_terrains(result, data_directory, global_ids);
    load_reliefs(result, data_directory, global_ids);
    load_features(result, data_directory, global_ids, feature_terrain_references);
    load_edges(result, data_directory, global_ids, feature_terrain_references);
    load_biome_rules(result, data_directory);
    load_movement_rules(result, data_directory);
    load_civilization_rules(result, data_directory);
    load_history_rules(result, data_directory);

    return result;
}

}  // namespace aetheria::rules

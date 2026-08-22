#include "core/rules/ruleset.h"

#include <set>
#include <utility>

#include "core/rules/toml_read.h"

namespace aetheria::rules {

using namespace detail;

const TerrainDef* Ruleset::terrain(TerrainId id) const noexcept { return lookup(terrains(), id); }
const ReliefDef* Ruleset::relief(ReliefId id) const noexcept { return lookup(reliefs(), id); }
const FeatureDef* Ruleset::feature(FeatureId id) const noexcept { return lookup(features(), id); }
const EdgeDef* Ruleset::edge(EdgeId id) const noexcept { return lookup(edges(), id); }
const GroundDef* Ruleset::ground(GroundId id) const noexcept { return lookup(grounds(), id); }
const BuildingDef* Ruleset::building(BuildingDefId id) const noexcept {
    return lookup(buildings(), id);
}
const CityBuildingDef* Ruleset::city_building(CityBuildingDefId id) const noexcept {
    return lookup(city_buildings(), id);
}
const FurnitureDef* Ruleset::furniture(FurnitureDefId id) const noexcept {
    return lookup(furniture(), id);
}
const PowerBreakthroughDef* Ruleset::breakthrough(PowerBreakthroughDefId id) const noexcept {
    return lookup(breakthroughs(), id);
}

const DamageTypeDef* Ruleset::damage_type(DamageTypeId id) const noexcept {
    return lookup(damage_types(), id);
}

const TreatyDef* Ruleset::treaty(TreatyDefId id) const noexcept {
    return lookup(std::span<const TreatyDef>{diplomacy_rules_.treaties}, id);
}
const CasusBelliDef* Ruleset::casus_belli(CasusBelliDefId id) const noexcept {
    return lookup(std::span<const CasusBelliDef>{diplomacy_rules_.casus_belli}, id);
}
const TerrainGroundMapping* Ruleset::terrain_ground_mapping(TerrainId id) const noexcept {
    return lookup(terrain_ground_mappings(), id);
}

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
std::optional<GroundId> Ruleset::find_ground(std::string_view id) const noexcept {
    return find_id(ground_index_, id);
}
std::optional<BuildingDefId> Ruleset::find_building(std::string_view id) const noexcept {
    return find_id(building_index_, id);
}
std::optional<CityBuildingDefId> Ruleset::find_city_building(std::string_view id) const noexcept {
    return find_id(city_building_index_, id);
}
std::optional<FurnitureDefId> Ruleset::find_furniture(std::string_view id) const noexcept {
    return find_id(furniture_index_, id);
}
std::optional<PowerBreakthroughDefId> Ruleset::find_breakthrough(std::string_view id) const noexcept {
    return find_id(breakthrough_index_, id);
}

std::optional<DamageTypeId> Ruleset::find_damage_type(std::string_view id) const noexcept {
    return find_id(damage_type_index_, id);
}

std::optional<TreatyDefId> Ruleset::find_treaty(std::string_view id) const noexcept {
    return find_id(treaty_index_, id);
}
std::optional<CasusBelliDefId> Ruleset::find_casus_belli(std::string_view id) const noexcept {
    return find_id(casus_belli_index_, id);
}

Ruleset RulesetLoader::load(const std::filesystem::path& data_directory) {
    Ruleset result;
    std::set<std::string, std::less<>> global_ids;
    std::vector<std::pair<std::size_t, std::string>> feature_terrain_references;

    load_terrains(result, data_directory, global_ids);
    load_reliefs(result, data_directory, global_ids);
    load_features(result, data_directory, global_ids, feature_terrain_references);
    load_edges(result, data_directory, global_ids, feature_terrain_references);
    load_grounds(result, data_directory, global_ids);
    load_site_projection(result, data_directory);
    load_site_city(result, data_directory, global_ids);
    load_site_build(result, data_directory, global_ids);
    load_site_wilderness(result, data_directory);
    load_local_buildings(result, data_directory, global_ids);
    load_individual_rules(result, data_directory, global_ids);
    load_biome_rule_tables(result, data_directory);
    load_movement_rules(result, data_directory);
    load_faction_rules(result, data_directory);
    load_civilization_rules(result, data_directory);
    load_history_rules(result, data_directory);
    load_diplomacy_rules(result, data_directory, global_ids);
    load_world_graph(result, data_directory);
    load_power_rules(result, data_directory, global_ids);
    load_combat_rules(result, data_directory);
    load_world_observation_rules(result, data_directory);
    load_power_source_rules(result, data_directory, global_ids);

    return result;
}

const SchoolDef* Ruleset::school(SchoolDefId id) const noexcept {
    return lookup(schools(), id);
}
const TenetDef* Ruleset::tenet(TenetDefId id) const noexcept { return lookup(tenets(), id); }
const DeityDef* Ruleset::deity(DeityDefId id) const noexcept { return lookup(deities(), id); }
const RaceDef* Ruleset::race(RaceDefId id) const noexcept { return lookup(races(), id); }
std::optional<SchoolDefId> Ruleset::find_school(std::string_view id) const noexcept {
    return find_id(school_index_, id);
}
std::optional<TenetDefId> Ruleset::find_tenet(std::string_view id) const noexcept {
    return find_id(tenet_index_, id);
}
std::optional<DeityDefId> Ruleset::find_deity(std::string_view id) const noexcept {
    return find_id(deity_index_, id);
}
std::optional<RaceDefId> Ruleset::find_race(std::string_view id) const noexcept {
    return find_id(race_index_, id);
}

}  // namespace aetheria::rules

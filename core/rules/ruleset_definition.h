#pragma once

// core/rules/ruleset.h：呼叫端唯一入口，彙整 def 型別與規則表，定義
// Ruleset／RulesetLoader。

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/rules/attributes.h"
#include "core/rules/check.h"
#include "core/rules/damage.h"
#include "core/rules/def_types.h"
#include "core/rules/power.h"
#include "core/rules/diplomacy_rules.h"
#include "core/rules/rule_tables.h"
#include "core/rules/site_build_rules.h"
#include "core/rules/combat.h"
#include "core/rules/world_observation_rules.h"
#include "core/rules/power_sources.h"
#include "core/rules/dungeon_rules.h"

namespace aetheria::rules {

class RulesetLoader;

// Ruleset 是 TOML 載入後不可變的 def 集合與字串索引。
// 世界狀態擁有它，其餘系統只借用 const Ruleset&。
// 擁有者析構後所有 def 指標、span 與執行期下標失效。
class Ruleset {
    public:
    Ruleset(const Ruleset&) = delete;
    Ruleset& operator=(const Ruleset&) = delete;
    Ruleset(Ruleset&&) noexcept = default;
    Ruleset& operator=(Ruleset&&) noexcept = delete;

    [[nodiscard]] const TerrainDef* terrain(TerrainId id) const noexcept;
    [[nodiscard]] const ReliefDef* relief(ReliefId id) const noexcept;
    [[nodiscard]] const FeatureDef* feature(FeatureId id) const noexcept;
    [[nodiscard]] const EdgeDef* edge(EdgeId id) const noexcept;
    [[nodiscard]] const GroundDef* ground(GroundId id) const noexcept;
    [[nodiscard]] const BuildingDef* building(BuildingDefId id) const noexcept;
    [[nodiscard]] const CityBuildingDef* city_building(CityBuildingDefId id) const noexcept;
    [[nodiscard]] const FurnitureDef* furniture(FurnitureDefId id) const noexcept;
    [[nodiscard]] const PowerBreakthroughDef* breakthrough(
        PowerBreakthroughDefId id) const noexcept;
    [[nodiscard]] const DamageTypeDef* damage_type(DamageTypeId id) const noexcept;
    [[nodiscard]] const TreatyDef* treaty(TreatyDefId id) const noexcept;
    [[nodiscard]] const CasusBelliDef* casus_belli(CasusBelliDefId id) const noexcept;
    [[nodiscard]] const TerrainGroundMapping* terrain_ground_mapping(TerrainId id) const noexcept;

    [[nodiscard]] std::optional<TerrainId> find_terrain(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<ReliefId> find_relief(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<FeatureId> find_feature(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<EdgeId> find_edge(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<GroundId> find_ground(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<BuildingDefId> find_building(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<CityBuildingDefId> find_city_building(
        std::string_view id) const noexcept;
    [[nodiscard]] std::optional<FurnitureDefId> find_furniture(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<PowerBreakthroughDefId> find_breakthrough(
        std::string_view id) const noexcept;
    [[nodiscard]] std::optional<DamageTypeId> find_damage_type(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<TreatyDefId> find_treaty(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<CasusBelliDefId> find_casus_belli(
        std::string_view id) const noexcept;

    [[nodiscard]] std::span<const TerrainDef> terrains() const noexcept { return terrains_; }
    [[nodiscard]] std::span<const ReliefDef> reliefs() const noexcept { return reliefs_; }
    [[nodiscard]] std::span<const FeatureDef> features() const noexcept { return features_; }
    [[nodiscard]] std::span<const EdgeDef> edges() const noexcept { return edges_; }
    [[nodiscard]] std::span<const GroundDef> grounds() const noexcept { return grounds_; }
    [[nodiscard]] std::span<const BuildingDef> buildings() const noexcept { return buildings_; }
    [[nodiscard]] std::span<const CityBuildingDef> city_buildings() const noexcept {
        return city_buildings_;
    }
    [[nodiscard]] std::span<const FurnitureDef> furniture() const noexcept { return furniture_; }
    [[nodiscard]] std::span<const PowerBreakthroughDef> breakthroughs() const noexcept {
        return breakthroughs_;
    }
    [[nodiscard]] std::span<const DamageTypeDef> damage_types() const noexcept {
        return damage_types_;
    }
    [[nodiscard]] std::span<const TerrainGroundMapping> terrain_ground_mappings() const noexcept {
        return terrain_ground_mappings_;
    }
    [[nodiscard]] std::span<const TerrainRule> terrain_rules() const noexcept {
        return terrain_rules_;
    }
    [[nodiscard]] std::span<const ReliefRule> relief_rules() const noexcept {
        return relief_rules_;
    }
    [[nodiscard]] const MovementRules& movement_rules() const noexcept { return movement_rules_; }
    [[nodiscard]] const SiteGenerationRules& site_generation_rules() const noexcept {
        return site_generation_rules_;
    }
    [[nodiscard]] const SiteFillRules& site_fill_rules() const noexcept { return site_fill_rules_; }
    [[nodiscard]] const SiteBuildRules& site_build_rules() const noexcept {
        return site_build_rules_;
    }
    [[nodiscard]] const WildernessGenerationRules& wilderness_generation_rules() const noexcept {
        return wilderness_generation_rules_;
    }
    [[nodiscard]] const LocalBuildingRules& local_building_rules() const noexcept {
        return local_building_rules_;
    }
    [[nodiscard]] const CivilizationRules& civilization_rules() const noexcept {
        return civilization_rules_;
    }
    [[nodiscard]] const PowerRules& power_rules() const noexcept { return power_rules_; }
    [[nodiscard]] const AttributeRules& attribute_rules() const noexcept {
        return attribute_rules_;
    }
    [[nodiscard]] const CheckRules& check_rules() const noexcept { return check_rules_; }
    [[nodiscard]] const DamageRules& damage_rules() const noexcept { return damage_rules_; }
    [[nodiscard]] const DiplomacyRules& diplomacy_rules() const noexcept {
        return diplomacy_rules_;
    }
    [[nodiscard]] std::span<const WorldGraphConnection> world_connections() const noexcept {
        return world_connections_;
    }
    [[nodiscard]] const CombatRules& combat_rules() const noexcept { return combat_rules_; }
    [[nodiscard]] const WorldObservationRules& world_observation_rules() const noexcept {
        return world_observation_rules_;
    }
    [[nodiscard]] const SchoolDef* school(SchoolDefId id) const noexcept;
    [[nodiscard]] const TenetDef* tenet(TenetDefId id) const noexcept;
    [[nodiscard]] const DeityDef* deity(DeityDefId id) const noexcept;
    [[nodiscard]] const RaceDef* race(RaceDefId id) const noexcept;
    [[nodiscard]] std::optional<SchoolDefId> find_school(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<TenetDefId> find_tenet(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<DeityDefId> find_deity(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<RaceDefId> find_race(std::string_view id) const noexcept;
    [[nodiscard]] std::span<const SchoolDef> schools() const noexcept { return schools_; }
    [[nodiscard]] std::span<const TenetDef> tenets() const noexcept { return tenets_; }
    [[nodiscard]] std::span<const DeityDef> deities() const noexcept { return deities_; }
    [[nodiscard]] std::span<const RaceDef> races() const noexcept { return races_; }
    [[nodiscard]] const PowerSourceRules& power_source_rules() const noexcept {
        return power_source_rules_;
    }
    [[nodiscard]] const TrapDef* trap(TrapDefId id) const noexcept;
    [[nodiscard]] std::optional<TrapDefId> find_trap(std::string_view id) const noexcept;
    [[nodiscard]] std::span<const TrapDef> traps() const noexcept { return traps_; }
    [[nodiscard]] const DungeonRules& dungeon_rules() const noexcept { return dungeon_rules_; }

#include "core/rules/ruleset_storage.inc"
};

}  // namespace aetheria::rules

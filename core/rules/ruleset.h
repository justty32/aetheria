#pragma once

// core/rules/ruleset.h：呼叫端唯一入口，彙整 def 型別與規則表，定義 Ruleset／RulesetLoader。

#include "core/rules/def_types.h"
#include "core/rules/rule_tables.h"
#include "core/rules/site_build_rules.h"

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
    [[nodiscard]] const TerrainGroundMapping* terrain_ground_mapping(TerrainId id) const noexcept;

    [[nodiscard]] std::optional<TerrainId> find_terrain(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<ReliefId> find_relief(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<FeatureId> find_feature(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<EdgeId> find_edge(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<GroundId> find_ground(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<BuildingDefId> find_building(std::string_view id) const noexcept;
    [[nodiscard]] std::optional<CityBuildingDefId> find_city_building(
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
    [[nodiscard]] const SiteFillRules& site_fill_rules() const noexcept {
        return site_fill_rules_;
    }
    [[nodiscard]] const SiteBuildRules& site_build_rules() const noexcept {
        return site_build_rules_;
    }
    [[nodiscard]] const WildernessGenerationRules& wilderness_generation_rules() const noexcept {
        return wilderness_generation_rules_;
    }
    [[nodiscard]] const CivilizationRules& civilization_rules() const noexcept {
        return civilization_rules_;
    }
    [[nodiscard]] std::span<const WorldGraphConnection> world_connections() const noexcept {
        return world_connections_;
    }

    private:
    friend class RulesetLoader;
    Ruleset() = default;

    std::vector<TerrainDef> terrains_;
    std::vector<ReliefDef> reliefs_;
    std::vector<FeatureDef> features_;
    std::vector<EdgeDef> edges_;
    std::vector<GroundDef> grounds_;
    std::vector<BuildingDef> buildings_;
    std::vector<CityBuildingDef> city_buildings_;
    std::vector<TerrainGroundMapping> terrain_ground_mappings_;
    std::vector<TerrainRule> terrain_rules_;
    std::vector<ReliefRule> relief_rules_;
    MovementRules movement_rules_;
    SiteGenerationRules site_generation_rules_;
    SiteFillRules site_fill_rules_;
    SiteBuildRules site_build_rules_;
    WildernessGenerationRules wilderness_generation_rules_;
    CivilizationRules civilization_rules_;
    std::vector<WorldGraphConnection> world_connections_;
    std::map<std::string, TerrainId, std::less<>> terrain_index_;
    std::map<std::string, ReliefId, std::less<>> relief_index_;
    std::map<std::string, FeatureId, std::less<>> feature_index_;
    std::map<std::string, EdgeId, std::less<>> edge_index_;
    std::map<std::string, GroundId, std::less<>> ground_index_;
    std::map<std::string, BuildingDefId, std::less<>> building_index_;
    std::map<std::string, CityBuildingDefId, std::less<>> city_building_index_;
};

// RulesetLoader 將一個 data 目錄完整解析成不可變 Ruleset。
// 呼叫端擁有回傳值，loader 不保留狀態。
// load 結束後沒有借用留在 loader 中。
class RulesetLoader {
    public:
    [[nodiscard]] static Ruleset load(const std::filesystem::path& data_directory);

    private:
    static void load_terrains(Ruleset& result, const std::filesystem::path& data_directory,
                              std::set<std::string, std::less<>>& global_ids);
    static void load_reliefs(Ruleset& result, const std::filesystem::path& data_directory,
                             std::set<std::string, std::less<>>& global_ids);
    static void load_features(Ruleset& result, const std::filesystem::path& data_directory,
                              std::set<std::string, std::less<>>& global_ids,
                              std::vector<std::pair<std::size_t, std::string>>&
                                  feature_terrain_references);
    static void load_edges(Ruleset& result, const std::filesystem::path& data_directory,
                           std::set<std::string, std::less<>>& global_ids,
                           std::vector<std::pair<std::size_t, std::string>>&
                               feature_terrain_references);
    static void load_grounds(Ruleset& result, const std::filesystem::path& data_directory,
                             std::set<std::string, std::less<>>& global_ids);
    static void load_site_projection(Ruleset& result, const std::filesystem::path& data_directory);
    static void load_site_city(Ruleset& result, const std::filesystem::path& data_directory,
                               std::set<std::string, std::less<>>& global_ids);
    static void load_site_build(Ruleset& result, const std::filesystem::path& data_directory,
                                std::set<std::string, std::less<>>& global_ids);
    static void load_site_wilderness(Ruleset& result,
                                     const std::filesystem::path& data_directory);
    static void load_biome_rule_tables(Ruleset& result,
                                       const std::filesystem::path& data_directory);
    static void load_movement_rules(Ruleset& result, const std::filesystem::path& data_directory);
    static void load_faction_rules(Ruleset& result,
                                   const std::filesystem::path& data_directory);
    static void load_civilization_rules(Ruleset& result,
                                        const std::filesystem::path& data_directory);
    static void load_history_rules(Ruleset& result,
                                   const std::filesystem::path& data_directory);
    static void load_crossing_rules(const Ruleset& result, CivilizationRules& rules);
    static void load_world_graph(Ruleset& result,
                                 const std::filesystem::path& data_directory);
};

}  // namespace aetheria::rules

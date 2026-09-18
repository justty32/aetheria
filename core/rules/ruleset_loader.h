#pragma once

// Ruleset 載入器宣告；私有階段各自定義於 ruleset_load_*.cpp。

#include "core/rules/ruleset_definition.h"

namespace aetheria::rules {

namespace detail {
struct DungeonTrapLoadContext;
struct PowerSourceLoadContext;
struct SiteCityLoadContext;
}  // namespace detail

// RulesetLoader 將一個 data 目錄完整解析成不可變 Ruleset。
// 呼叫端擁有回傳值，loader 不保留狀態。
// load 結束後沒有借用留在 loader 中。
class RulesetLoader {
    public:
    [[nodiscard]] static Ruleset load(const std::filesystem::path& data_directory);

    private:
    static void load_dungeon_traps(Ruleset& result,
                                    const detail::DungeonTrapLoadContext& context);
    static void load_power_source_defs(Ruleset& result,
                                        const detail::PowerSourceLoadContext& context);
    static void load_site_city_styles(Ruleset& result,
                                      const detail::SiteCityLoadContext& context);
    static void load_site_city_fortification(Ruleset& result,
                                             const detail::SiteCityLoadContext& context);
    static void load_terrains(Ruleset& result, const std::filesystem::path& data_directory,
                              std::set<std::string, std::less<>>& global_ids);
    static void load_reliefs(Ruleset& result, const std::filesystem::path& data_directory,
                             std::set<std::string, std::less<>>& global_ids);
    static void load_features(
        Ruleset& result, const std::filesystem::path& data_directory,
        std::set<std::string, std::less<>>& global_ids,
        std::vector<std::pair<std::size_t, std::string>>& feature_terrain_references);
    static void load_edges(
        Ruleset& result, const std::filesystem::path& data_directory,
        std::set<std::string, std::less<>>& global_ids,
        std::vector<std::pair<std::size_t, std::string>>& feature_terrain_references);
    static void load_grounds(Ruleset& result, const std::filesystem::path& data_directory,
                             std::set<std::string, std::less<>>& global_ids);
    static void load_site_projection(Ruleset& result, const std::filesystem::path& data_directory);
    static void load_site_city(Ruleset& result, const std::filesystem::path& data_directory,
                               std::set<std::string, std::less<>>& global_ids);
    static void load_site_build(Ruleset& result, const std::filesystem::path& data_directory,
                                std::set<std::string, std::less<>>& global_ids);
    static void load_site_wilderness(Ruleset& result, const std::filesystem::path& data_directory);
    static void load_local_buildings(Ruleset& result, const std::filesystem::path& data_directory,
                                     std::set<std::string, std::less<>>& global_ids);
    static void load_individual_rules(Ruleset& result, const std::filesystem::path& data_directory,
                                      std::set<std::string, std::less<>>& global_ids);
    static void load_biome_rule_tables(Ruleset& result,
                                       const std::filesystem::path& data_directory);
    static void load_movement_rules(Ruleset& result, const std::filesystem::path& data_directory);
    static void load_faction_rules(Ruleset& result, const std::filesystem::path& data_directory);
    static void load_civilization_rules(Ruleset& result,
                                        const std::filesystem::path& data_directory);
    static void load_history_rules(Ruleset& result, const std::filesystem::path& data_directory);
    static void load_diplomacy_rules(Ruleset& result,
                                     const std::filesystem::path& data_directory,
                                     std::set<std::string, std::less<>>& global_ids);
    static void load_crossing_rules(const Ruleset& result, CivilizationRules& rules);
    static void load_world_graph(Ruleset& result, const std::filesystem::path& data_directory);
    static void load_power_rules(Ruleset& result, const std::filesystem::path& data_directory,
                                 std::set<std::string, std::less<>>& global_ids);
    static void load_combat_rules(Ruleset& result, const std::filesystem::path& data_directory);
    static void load_world_observation_rules(Ruleset& result,
                                             const std::filesystem::path& data_directory);
    static void load_power_source_rules(Ruleset& result,
                                        const std::filesystem::path& data_directory,
                                        std::set<std::string, std::less<>>& global_ids);
    static void load_dungeon_rules(Ruleset& result, const std::filesystem::path& data_directory,
                                   std::set<std::string, std::less<>>& global_ids);
};

}  // namespace aetheria::rules

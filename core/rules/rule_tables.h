#pragma once

// core/rules/rule_tables.h：biome／movement／civilization／Local 生成規則表型別。

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "core/rules/def_types.h"

namespace aetheria::rules {

// TerrainRule 是群系在溫度／水氣／高度空間中的整數計分中心。
// scale 為 0 的軸不參與；Ruleset 擁有所有實例，生成器只借用 const span。
// 所屬 Ruleset 析構後失效；同分由規則下標小者勝出。
struct TerrainRule {
    std::int16_t temperature_target_tenths{};
    std::uint16_t temperature_scale_tenths{};
    std::uint16_t moisture_target{};
    std::uint16_t moisture_scale{};
    std::uint16_t elevation_target{};
    std::uint16_t elevation_scale{};
    std::int32_t score_bias{};
    TerrainId terrain;
};

// ReliefRule 是資料檔中依序第一命中的起伏規則。
// 型別刻意只容納 elevation／ruggedness，氣候值不能進入地貌裁決。
// 所屬 Ruleset 析構後失效；fallback 必須是最後一條。
struct ReliefRule {
    std::uint16_t min_elevation{};
    std::uint16_t max_elevation{std::numeric_limits<std::uint16_t>::max()};
    std::uint16_t min_ruggedness{};
    std::uint16_t max_ruggedness{std::numeric_limits<std::uint16_t>::max()};
    ReliefId relief;
    bool fallback{};
};

// MovementRules 是資料檔提供的四季整數移動倍率。
// Ruleset 擁有值，移動與尋路系統只讀取複本。
// Ruleset 析構後失效；numerator／denominator 皆以整數計算並向上取整。
struct MovementRules {
    std::array<std::uint16_t, 4> season_numerators{};
    std::uint16_t season_denominator{};
    bool loaded{};
};

// CrossingRule 將河級與道路級資料驅動映射成單一複合 EdgeDef。
// CivilizationRules 擁有所有實例，worldgen 只讀取複本。
// 所屬 Ruleset 析構後失效。
struct CrossingRule {
    EdgeId river;
    EdgeId road;
    EdgeId result;
};

// SiteGenerationRules 是城區骨架 S1～S4 的資料驅動限制。
// Ruleset 擁有值，Site 生成器只讀取複本；比例皆為整數百分比。
struct SiteGenerationRules {
    std::uint8_t block_split_depth{};
    std::uint8_t block_cut_min_percent{};
    std::uint8_t block_cut_max_percent{};
    std::uint8_t block_min_extent{};
    std::uint16_t height_noise_amplitude{};
    std::uint16_t max_buildable_slope{};
    std::uint8_t water_inland_reach{};
    bool loaded{};
};

// WildernessGenerationRules 是荒野 W1～W6 的資料驅動密度與有界成本參數。
// 百分比皆為 0～100；Ruleset 擁有值，荒野生成器只讀取複本。
struct WildernessGenerationRules {
    std::uint16_t height_noise_amplitude{};
    std::uint16_t plain_passable_slope{};
    std::uint16_t hills_passable_slope{};
    std::uint16_t mountain_passable_slope{};
    std::uint8_t jitter_cell_extent{};
    std::uint8_t sparse_vegetation_percent{};
    std::uint8_t forest_vegetation_percent{};
    std::uint8_t base_resource_points{};
    std::uint8_t mine_resource_points{};
    std::uint8_t owned_encounter_points{};
    std::uint8_t unowned_encounter_points{};
    std::uint8_t road_traveler_points{};
    std::uint8_t wilderness_portals{};
    std::uint8_t mountain_portals{};
    std::uint8_t ruin_portals{};
    std::uint8_t ruin_keep_min_percent{};
    std::uint8_t ruin_keep_max_percent{};
    bool loaded{};
};

enum class LocalRoomKind : std::uint8_t {
    Bedroom,
    Kitchen,
    Workshop,
    Shop,
};

struct FurnitureDef {
    std::string id;
    LocalRoomKind room{LocalRoomKind::Bedroom};
    std::uint8_t minimum{};
    std::uint8_t maximum{};
};

// LocalBuildingRules 是 L3 路線 A 的幾何、邊材質、垂直層與居民統計規則。
struct LocalBuildingRules {
    std::uint8_t house_margin{};
    std::uint8_t house_depth{};
    std::uint8_t house_frontage_min{};
    std::uint8_t house_frontage_max{};
    std::uint8_t room_split_depth{};
    std::uint8_t room_cut_min_percent{};
    std::uint8_t room_cut_max_percent{};
    std::uint8_t room_min_extent{};
    std::uint8_t upper_floor_percent{};
    std::uint8_t cellar_percent{};
    std::uint8_t residents_min{};
    std::uint8_t residents_max{};
    GroundId foundation_ground{};
    EdgeId wall_edge{};
    EdgeId residential_door_edge{};
    EdgeId commercial_door_edge{};
    EdgeId window_edge{};
    bool loaded{};
};

// SiteFillZone 目前只列出能由既有快變數驅動的 F1 分區。
enum class SiteFillZone : std::uint8_t {
    Residential,
    Commercial,
};

enum class SiteQuotaDriver : std::uint8_t {
    Population,
    DevelopmentLevel,
};

struct SiteZoneQuota {
    SiteFillZone zone{SiteFillZone::Residential};
    SiteQuotaDriver driver{SiteQuotaDriver::Population};
    std::uint32_t units_per_block{};
    std::uint8_t max_percent{};
};

// UndergroundKind 由 Site 的 structure def 決定 Local 路線 C 種類；None 仍走路線 A。
enum class UndergroundKind : std::uint8_t {
    None,
    Mine,
    Dungeon,
    Ruin,
};

struct BuildingDef {
    std::string id;
    SiteFillZone zone{SiteFillZone::Residential};
    std::uint8_t frontage{};
    std::uint8_t depth{};
    bool landmark{};
    UndergroundKind underground{UndergroundKind::None};
    std::uint8_t underground_depth{};
};

struct FactionLandmarkStyle {
    std::uint16_t faction{};
    std::vector<BuildingDefId> landmarks;
};

struct SiteFortificationRules {
    std::uint16_t double_wall_defense{};
    std::uint16_t tower_defense{};
    std::uint8_t tower_spacing{};
    std::uint16_t moat_defense{};
    std::uint8_t breach_percent_at_full_damage{};
    EdgeId wall_edge{};
    EdgeId gate_edge{};
    EdgeId tower_edge{};
    EdgeId moat_edge{};
};

// SiteFillRules 是 F1～F5 城區填充的資料驅動規則。
struct SiteFillRules {
    std::vector<SiteZoneQuota> quotas;
    std::vector<FactionLandmarkStyle> faction_styles;
    SiteFortificationRules fortification;
    std::uint8_t base_density_percent{};
    std::uint8_t development_density_per_level{};
    std::uint8_t max_density_percent{};
    bool loaded{};
};

// SettlementScoringWeights 是同一套選址因子的整數權重。
// 現代與上古文明各自持有一份，worldgen 評分函式只借用 const 參考。
struct SettlementScoringWeights {
    std::int32_t freshwater{};
    std::int32_t farmland{};
    std::int32_t harbor{};
    std::int32_t defense{};
    std::int32_t resource{};
    std::int32_t bottleneck{};
    std::int32_t extreme_climate_penalty{};
    std::int32_t high_elevation_penalty{};
};

// WorldConnectionType 決定 WorldGraph 通道在 Region 內解析 portal 的規則。
enum class WorldConnectionType : std::uint8_t {
    SeaRoute,
    MountainPass,
    Underground,
    Teleport,
};

// WorldConnectionEndpoint 是傳送門可選的資料指定座標。
// WorldGraphConnection 擁有值；其他通道不得指定座標。
struct WorldConnectionEndpoint {
    std::int16_t x{};
    std::int16_t y{};

    constexpr bool operator==(const WorldConnectionEndpoint&) const noexcept = default;
};

// WorldGraphConnection 是 world_graph.toml 的一條手工通道宣告。
// Ruleset 擁有所有實例，worldgen 階段 11 只借用 const 參考。
// connection id 是跨宣告順序穩定的資料識別。
struct WorldGraphConnection {
    WorldConnectionId id{};
    std::uint32_t region_a{};
    std::uint32_t region_b{};
    WorldConnectionType type{WorldConnectionType::SeaRoute};
    std::uint32_t cost_ticks{};
    std::string requirement;
    std::optional<WorldConnectionEndpoint> coordinate_a;
    std::optional<WorldConnectionEndpoint> coordinate_b;

    bool operator==(const WorldGraphConnection&) const noexcept = default;
};

// CivilizationRules 是城市評分、間距、道路工程與渡河查表的資料規則。
// Ruleset 擁有值，worldgen 階段 8～10 只借用 const 參考。
// 所屬 Ruleset 析構後失效；所有分數與成本均為整數。
struct CivilizationRules {
    // HistoryRules 是上古選址、災變、古道與現代回饋的資料規則。
    // CivilizationRules 擁有值，worldgen 階段 8～10 只借用 const 參考。
    // 所屬 Ruleset 析構後失效；三級陣列依序為村莊、城鎮、大城。
    struct HistoryRules {
        SettlementScoringWeights scoring_weights{};
        std::uint16_t ancient_site_count{};
        std::uint16_t ancient_city_count{};
        std::uint16_t ancient_town_count{};
        std::array<std::uint16_t, 3> minimum_spacing{};
        std::uint8_t survivor_percent{};
        std::int32_t ancient_site_bonus{};
        std::uint16_t ancient_road_reuse_numerator{};
        std::uint16_t ancient_road_reuse_denominator{};
        EdgeId road_edge{};
        std::array<FeatureId, 3> ruin_features{};
    };

    // FactionRules 是階段 12 的權威勢力數、治理距離與影響力季節。
    // CivilizationRules 擁有值，worldgen 只借用 const 參考。
    struct FactionRules {
        std::uint16_t faction_count{};
        std::int64_t governance_max_cost{};
        std::uint8_t influence_season{};
    };

    struct FactionAiRules {
        struct FactionDef {
            std::uint16_t faction{};
            std::string id;
            std::int32_t expansion{};
            std::int32_t aggression{};
            std::int32_t fidelity{};
            std::int32_t commerce{};
            std::int32_t piety{};
            std::int32_t caution{};
            std::int32_t resentment{};
        };
        std::int32_t goal_switch_threshold{};
        std::int32_t full_ai_field_threshold{};
        std::int32_t marked_observer_strength{};
        std::int32_t war_observer_strength{};
        std::vector<FactionDef> definitions;
    };

    SettlementScoringWeights scoring_weights{};
    std::uint16_t high_elevation_threshold{};
    std::uint16_t target_city_count{};
    std::uint16_t major_city_count{};
    std::uint16_t town_count{};
    std::array<std::uint16_t, 3> minimum_spacing{};
    std::uint8_t bottleneck_radius{};
    std::uint16_t bottleneck_barrier_move_cost{};
    std::uint8_t loop_percent{};
    std::uint16_t road_base_cost{};
    std::uint16_t road_terrain_weight{};
    std::uint16_t road_slope_weight{};
    std::uint16_t road_slope_divisor{};
    std::uint16_t road_valley_discount{};
    std::uint16_t road_swamp_penalty{};
    std::uint16_t road_river_crossing_penalty{};
    std::uint16_t road_reuse_numerator{};
    std::uint16_t road_reuse_denominator{};
    std::array<std::uint16_t, 3> road_usage_thresholds{};
    std::array<EdgeId, 3> road_edges{};
    TerrainId swamp_terrain;
    std::vector<CrossingRule> crossings;
    HistoryRules history{};
    FactionRules factions{};
    FactionAiRules faction_ai{};
    bool loaded{};
};

}  // namespace aetheria::rules

#pragma once

// core/rules/rule_tables.h：biome／movement／civilization 規則表型別。

#include "core/rules/def_types.h"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace aetheria::rules {

// BiomeRule 是資料檔中依序第一命中的地形／起伏生成規則。
// Ruleset 擁有所有實例，生成器只借用 const span。
// 所屬 Ruleset 析構後失效；fallback 必須是最後一條。
struct BiomeRule {
    std::int16_t min_temperature_tenths{std::numeric_limits<std::int16_t>::min()};
    std::int16_t max_temperature_tenths{std::numeric_limits<std::int16_t>::max()};
    std::uint16_t min_moisture{};
    std::uint16_t max_moisture{std::numeric_limits<std::uint16_t>::max()};
    std::uint16_t min_elevation{};
    std::uint16_t max_elevation{std::numeric_limits<std::uint16_t>::max()};
    std::uint16_t min_ruggedness{};
    std::uint16_t max_ruggedness{std::numeric_limits<std::uint16_t>::max()};
    TerrainId terrain;
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

    // FactionRules 是階段 12 的權威勢力數與影響力預算。
    // CivilizationRules 擁有值，worldgen 只借用 const 參考。
    struct FactionRules {
        std::uint16_t faction_count{};
        std::int64_t influence_max_cost{};
        std::uint8_t influence_season{};
    };

    SettlementScoringWeights scoring_weights{};
    std::uint16_t high_elevation_threshold{};
    std::uint16_t target_city_count{};
    std::uint16_t major_city_count{};
    std::uint16_t town_count{};
    std::array<std::uint16_t, 3> minimum_spacing{};
    std::uint8_t bottleneck_radius{};
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
    bool loaded{};
};

}  // namespace aetheria::rules

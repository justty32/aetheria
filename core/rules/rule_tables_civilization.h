#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "core/rules/def_types.h"

#include "core/rules/rule_tables.h"

namespace aetheria::rules {

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

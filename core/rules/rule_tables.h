#pragma once

// core/rules/rule_tables.h：biome／movement／civilization 規則表型別。

#include "core/rules/def_types.h"

#include <array>
#include <cstdint>
#include <limits>
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

// CivilizationRules 是城市評分、間距、道路工程與渡河查表的資料規則。
// Ruleset 擁有值，worldgen 階段 8～9 只借用 const 參考。
// 所屬 Ruleset 析構後失效；所有分數與成本均為整數。
struct CivilizationRules {
    std::int32_t freshwater_weight{};
    std::int32_t farmland_weight{};
    std::int32_t harbor_weight{};
    std::int32_t defense_weight{};
    std::int32_t resource_weight{};
    std::int32_t bottleneck_weight{};
    std::int32_t extreme_climate_penalty{};
    std::int32_t high_elevation_penalty{};
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
    bool loaded{};
};

}  // namespace aetheria::rules

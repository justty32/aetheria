#pragma once

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

}  // namespace aetheria::rules

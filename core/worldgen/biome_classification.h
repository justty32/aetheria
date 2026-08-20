#pragma once

// biome_classification.h：terrain／relief 兩個正交規則表的型別隔離查詢入口。

#include "core/rules/ruleset.h"

#include <cstdint>

namespace aetheria::worldgen {

// TerrainClassificationInput 是群系裁決可見的氣候與高度資料。
// swamp 可依高度裁決；ruggedness 刻意不在此型別中。
struct TerrainClassificationInput {
    std::int16_t temperature_tenths{};
    std::uint16_t moisture{};
    std::uint16_t elevation{};
};

// ReliefClassificationInput 是地貌裁決唯一可見的資料。
// temperature／moisture 刻意不在此型別中，不能傳入 relief 規則。
struct ReliefClassificationInput {
    std::uint16_t elevation{};
    std::uint16_t ruggedness{};
};

[[nodiscard]] rules::TerrainId classify_terrain(const rules::Ruleset& ruleset,
                                                TerrainClassificationInput input);
[[nodiscard]] rules::ReliefId classify_relief(const rules::Ruleset& ruleset,
                                              ReliefClassificationInput input);

}  // namespace aetheria::worldgen

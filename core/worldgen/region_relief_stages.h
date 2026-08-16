#pragma once

// region_relief_stages.h 收斂板塊、高度場、侵蝕與量化四個地形階段的型別與函式宣告。

#include "core/worldgen/region_config.h"

#include <cstdint>
#include <vector>

namespace aetheria::worldgen {

// Plate 是一個 Voronoi 板塊的種子與慢變地質屬性。
// PlateStageOutput 擁有所有實例。
// 所屬輸出析構或 plates vector 重配後失效。
struct Plate {
    std::uint16_t x{};
    std::uint16_t y{};
    bool is_oceanic{};
    std::int8_t drift_x{};
    std::int8_t drift_y{};
    std::int16_t base_elevation{};
};

// PlateBoundaryType 是相鄰板塊依相對漂移判定的邊界類別。
// PlateStageOutput 擁有每格的值複本。
// 值本身永不失效；None 代表該格不在板塊邊界。
enum class PlateBoundaryType : std::uint8_t {
    None,
    Convergent,
    Divergent,
    Transform,
};

// PlateStageOutput 是板塊階段的完整、可視覺化產物。
// build_skeleton 的回傳值擁有它。
// 所屬回傳值析構或 vector 重配後其中參考失效。
struct PlateStageOutput {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<Plate> plates;
    std::vector<std::uint8_t> plate_index;
    std::vector<PlateBoundaryType> boundary_type;
    std::vector<std::int16_t> boundary_effect;
};

// HeightStageOutput 是量化前的浮點高度場與連通陸地遮罩。
// build_skeleton 的回傳值擁有它，不得直接寫進世界狀態。
// 所屬回傳值析構或 vector 重配後其中參考失效。
struct HeightStageOutput {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<double> elevation;
    std::vector<std::uint8_t> land;
    double sea_level{};
};

// ErosionStageOutput 是固定次數熱力侵蝕後的浮點中間產物。
// build_skeleton 的回傳值擁有它，不得直接交給 populate。
// 所屬回傳值析構或 vector 重配後其中參考失效。
struct ErosionStageOutput {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<double> elevation;
    std::vector<std::uint8_t> land;
    double sea_level{};
};

// QuantizedElevation 是唯一量化閘口產生的整數高度場。
// RegionSkeleton 擁有它，populate 只讀取其值。
// 所屬 skeleton 析構或 vector 重配後其中參考失效。
struct QuantizedElevation {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint16_t> meters;
    std::vector<std::uint8_t> land;
    std::uint16_t sea_level{};
};

[[nodiscard]] PlateStageOutput generate_plates(const RegionSlowVariables& slow,
                                               std::uint64_t stage_seed,
                                               const PlateGenerationConfig& config);
[[nodiscard]] HeightStageOutput generate_height(const PlateStageOutput& plates,
                                                std::uint64_t stage_seed,
                                                const HeightGenerationConfig& config);
[[nodiscard]] ErosionStageOutput erode_height(const HeightStageOutput& height,
                                              std::uint64_t stage_seed,
                                              const ErosionGenerationConfig& config);

// quantize_elevation 是浮點生成產物進入世界狀態前的唯一轉換點。
// 呼叫端擁有回傳的整數值，函式只借用 erosion。
// erosion 或回傳值析構後，各自內部參考失效，兩者互不借用。
[[nodiscard]] QuantizedElevation quantize_elevation(const ErosionStageOutput& erosion);

}  // namespace aetheria::worldgen

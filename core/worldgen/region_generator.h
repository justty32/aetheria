#pragma once

#include "core/rules/ruleset.h"
#include "core/world/region_tiles.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace aetheria::worldgen {

// RegionSlowVariables 是建立 Region 穩定骨架時可讀的慢變數。
// 世界狀態擁有值，生成器只在呼叫期間借用。
// 值本身永不失效；改動後必須明確重建骨架。
struct RegionSlowVariables {
    std::uint32_t region_id{};
    std::uint32_t width{128};
    std::uint32_t height{96};
};

// RegionFastVariables 是 populate 專用的快變數入口。
// 世界狀態擁有值，populate 只在呼叫期間借用。
// M1.1 尚無欄位；型別刻意不會出現在 build_skeleton 簽章。
struct RegionFastVariables {};

// PlateGenerationConfig 是板塊階段可調但不進世界狀態的參數。
// 呼叫端擁有值，各次生成只借用它。
// 呼叫結束後即可失效。
struct PlateGenerationConfig {
    std::uint8_t min_count{8};
    std::uint8_t max_count{16};
};

// HeightGenerationConfig 是高度場階段的噪聲與陸地比例參數。
// 呼叫端擁有值，各次生成只借用它。
// 呼叫結束後即可失效。
struct HeightGenerationConfig {
    std::uint8_t noise_octaves{6};
    std::uint8_t target_land_percent{30};
};

// ErosionGenerationConfig 是固定次數熱力侵蝕的參數。
// 呼叫端擁有值，各次生成只借用它。
// 呼叫結束後即可失效。
struct ErosionGenerationConfig {
    std::uint16_t iterations{12};
    double talus{120.0};
    double transfer_fraction{0.18};
};

// ClimateGenerationConfig 是固定點氣候與線性雨影的參數。
// 呼叫端擁有值，氣候階段只在呼叫期間借用。
// 呼叫結束後即可失效。
struct ClimateGenerationConfig {
    std::uint16_t lapse_tenths_per_km{65};
    std::uint16_t air_decay{1800};
    std::uint16_t uplift_rain{24};
};

// RiverGenerationConfig 是河網分級與水氣回灌參數。
// 呼叫端擁有值，河流階段只在呼叫期間借用。
// 呼叫結束後即可失效。
struct RiverGenerationConfig {
    std::uint32_t stream_threshold{90000};
    std::uint32_t river_threshold{180000};
    std::uint32_t great_river_threshold{420000};
    std::uint16_t moisture_bonus{9000};
};

// BiomeGenerationConfig 是資料表查詢前的可調整偏移。
// 呼叫端擁有值，biome 階段只在呼叫期間借用。
// 呼叫結束後即可失效。
struct BiomeGenerationConfig {
    std::int16_t temperature_bias_tenths{};
    std::int16_t moisture_bias{};
};

// FeatureGenerationConfig 是藍噪聲地物散布的密度參數。
// 呼叫端擁有值，地物階段只在呼叫期間借用。
// 呼叫結束後即可失效。
struct FeatureGenerationConfig {
    std::uint16_t forest_density_scale{42000};
    std::uint16_t mine_chance{9000};
    std::uint16_t oasis_chance{5000};
    std::uint16_t landmark_chance{180};
};

// RegionGenerationConfig 將七階段參數分槽，避免後段參數污染前段。
// 呼叫端擁有值，各階段只借用自己的子設定。
// 呼叫結束後即可失效。
struct RegionGenerationConfig {
    PlateGenerationConfig plates;
    HeightGenerationConfig height;
    ErosionGenerationConfig erosion;
    ClimateGenerationConfig climate;
    RiverGenerationConfig rivers;
    BiomeGenerationConfig biome;
    FeatureGenerationConfig features;
};

// GenerationParameterHashes 是 manifest 固定的七階段參數身分。
// SaveManifest 擁有值，FileZoneStore 與生成器只讀取複本。
// 值本身永不失效；任一分組不同即不得載入既有世界。
struct GenerationParameterHashes {
    std::array<std::uint64_t, 7> groups{};

    constexpr bool operator==(const GenerationParameterHashes&) const noexcept = default;
};

[[nodiscard]] GenerationParameterHashes generation_parameter_hashes(
    const RegionGenerationConfig& config = {}) noexcept;
[[nodiscard]] constexpr std::string_view generation_parameter_group_name(
    std::size_t index) noexcept {
    constexpr std::array names{"plates", "height", "erosion", "climate", "rivers", "biome",
                               "features"};
    return index < names.size() ? names[index] : "unknown";
}

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

// RegionDefinitionIds 是生成器啟動時一次解析完成的 Ruleset 下標。
// RegionSkeleton 擁有值，populate 只讀取複本。
// 所屬 skeleton 析構後失效；跨 Ruleset 不可沿用。
struct RegionDefinitionIds {
    rules::TerrainId land;
    rules::TerrainId ocean;
    rules::ReliefId plain;
    rules::FeatureId no_feature;
    rules::EdgeId no_edge;
};

// RegionSkeleton 是只含已量化慢變地形的穩定 Region 骨架。
// RegionBuildResult 擁有它，之後可移交世界狀態。
// 所屬擁有者析構或欄位重配後其中參考失效。
struct RegionSkeleton {
    QuantizedElevation elevation;
    RegionDefinitionIds definitions;
};

// RegionBuildResult 同時帶穩定骨架與三階段除錯產物。
// 呼叫端擁有整個回傳值。
// 回傳值析構後所有 stage 與 skeleton 參考失效。
struct RegionBuildResult {
    PlateStageOutput plates;
    HeightStageOutput height;
    ErosionStageOutput erosion;
    RegionSkeleton skeleton;
};

[[nodiscard]] std::uint64_t splitmix64(std::uint64_t value) noexcept;
[[nodiscard]] std::uint64_t derive_stage_seed(std::uint64_t world_seed,
                                              std::uint64_t stage_id) noexcept;
[[nodiscard]] std::uint64_t derive_region_seed(std::uint64_t world_seed,
                                               std::uint32_t region_id) noexcept;
[[nodiscard]] std::uint64_t derive_region_stage_seed(std::uint64_t world_seed,
                                                     std::uint32_t region_id,
                                                     std::uint64_t stage_id) noexcept;

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

[[nodiscard]] RegionBuildResult build_skeleton(const RegionSlowVariables& slow,
                                                std::uint64_t world_seed,
                                                const rules::Ruleset& ruleset,
                                                const RegionGenerationConfig& config = {});
[[nodiscard]] world::RegionTiles populate(const RegionSkeleton& skeleton,
                                          const RegionFastVariables& fast);

[[nodiscard]] std::uint64_t hash_stage(const PlateStageOutput& stage) noexcept;
[[nodiscard]] std::uint64_t hash_stage(const HeightStageOutput& stage) noexcept;
[[nodiscard]] std::uint64_t hash_stage(const ErosionStageOutput& stage) noexcept;
[[nodiscard]] std::uint64_t hash_skeleton(const RegionSkeleton& skeleton) noexcept;
[[nodiscard]] std::uint64_t hash_tiles(const world::RegionTiles& tiles) noexcept;
[[nodiscard]] double land_fraction(const RegionSkeleton& skeleton) noexcept;
[[nodiscard]] bool land_is_single_component(const RegionSkeleton& skeleton);

[[nodiscard]] std::vector<std::uint8_t> grayscale(const PlateStageOutput& stage);
[[nodiscard]] std::vector<std::uint8_t> grayscale(const HeightStageOutput& stage);
[[nodiscard]] std::vector<std::uint8_t> grayscale(const ErosionStageOutput& stage);

}  // namespace aetheria::worldgen

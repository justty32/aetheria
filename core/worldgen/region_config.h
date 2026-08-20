#pragma once

// region_config.h 收斂 Region 生成器的慢變數入口與十二階段可調參數。

#include <array>
#include <cstdint>
#include <string_view>

namespace aetheria::worldgen {

// RegionSlowVariables 是建立 Region 穩定骨架時可讀的慢變數。
// 世界狀態擁有值，生成器只在呼叫期間借用。
// 值本身永不失效；改動後必須明確重建骨架。
struct RegionSlowVariables {
    constexpr RegionSlowVariables(std::uint32_t value_region_id = 0,
                                  std::uint32_t value_width = 128, std::uint32_t value_height = 96,
                                  std::int16_t value_latitude_degrees = 35) noexcept
        : region_id{value_region_id}, width{value_width}, height{value_height},
          latitude_degrees{value_latitude_degrees} {}

    std::uint32_t region_id{};
    std::uint32_t width{128};
    std::uint32_t height{96};
    std::int16_t latitude_degrees{35};
};

// RegionFastVariables 是 populate 專用的快變數入口。
// 世界狀態擁有值，populate 只在呼叫期間借用。
// 目前尚無欄位；型別刻意不會出現在 build_skeleton 簽章。
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

// HistoryGenerationConfig 是階段 8 的整體分數偏移探針。
// 呼叫端擁有值；上古數量、間距、存活率與回饋仍全部來自 civilization.toml。
// 呼叫結束後即可失效。
struct HistoryGenerationConfig {
    std::int16_t minimum_score_bias{};
};

// CityGenerationConfig 是階段 9 的整體分數偏移探針。
// 呼叫端擁有值；各因子權重仍全部來自 civilization.toml。
// 呼叫結束後即可失效。
struct CityGenerationConfig {
    std::int16_t minimum_score_bias{};
};

// RoadGenerationConfig 是階段 10 的環路比例覆寫探針，0 表示採資料檔值。
// 呼叫端擁有值；道路工程成本仍全部來自 civilization.toml。
// 呼叫結束後即可失效。
struct RoadGenerationConfig {
    std::uint8_t loop_percent_override{};
};

// PortalGenerationConfig 是階段 11 補路時採用的道路級別。
// 呼叫端擁有值；WorldGraph 宣告與落點規則仍來自 Ruleset。
struct PortalGenerationConfig {
    std::uint8_t road_tier{};
};

// FactionGenerationConfig 是階段 12 配發連續勢力 id 的起點。
// 勢力數、影響力預算與季節全部來自 civilization.toml [factions]。
struct FactionGenerationConfig {
    std::uint16_t first_faction_id{1};
};

// RegionGenerationConfig 將十二階段參數分槽，避免後段參數污染前段。
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
    HistoryGenerationConfig history;
    CityGenerationConfig cities;
    RoadGenerationConfig roads;
    PortalGenerationConfig portals;
    FactionGenerationConfig factions;
};

// GenerationParameterHashes 是 manifest 固定的十二階段參數身分。
// SaveManifest 擁有值，FileZoneStore 與生成器只讀取複本。
// 值本身永不失效；任一分組不同即不得載入既有世界。
struct GenerationParameterHashes {
    std::array<std::uint64_t, 12> groups{};

    constexpr bool operator==(const GenerationParameterHashes&) const noexcept = default;
};

[[nodiscard]] GenerationParameterHashes
generation_parameter_hashes(const RegionGenerationConfig& config = {}) noexcept;
[[nodiscard]] constexpr std::string_view
generation_parameter_group_name(std::size_t index) noexcept {
    constexpr std::array names{"plates",   "height", "erosion", "climate",
                               "rivers",   "biome",  "features", "history",
                               "cities",   "roads",  "portals",  "factions"};
    return index < names.size() ? names[index] : "unknown";
}

}  // namespace aetheria::worldgen

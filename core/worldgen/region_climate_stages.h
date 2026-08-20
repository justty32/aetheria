#pragma once

// region_climate_stages.h 收斂氣候、河流、biome
// 與地物四個階段的型別與函式宣告。

#include "core/rules/ruleset.h"
#include "core/worldgen/region_config.h"
#include "core/worldgen/region_relief_stages.h"

#include <cstdint>
#include <vector>

namespace aetheria::worldgen {

// RegionDefinitionIds 定義於 region_skeleton.h；此處僅需引用型別供函式宣告的
// const 參考使用。
struct RegionDefinitionIds;

// ClimateStageOutput 是固定點溫度、地表濕度與緯向風力百分比的產物。
// prevailing_wind_x：-100 為向西、100 為向東，換向帶中間以 0 平滑過渡。
// build_skeleton 的回傳值擁有它，不含任何浮點欄位。
// 所屬回傳值析構或 vector 重配後其中參考失效。
struct ClimateStageOutput {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::int16_t> temperature_tenths;
    std::vector<std::uint16_t> moisture;
    std::vector<std::int8_t> prevailing_wind_x;
};

// RiverStageOutput 是 priority-flood 流向、流量與河級的整數產物。
// build_skeleton 的回傳值擁有它，不含任何浮點欄位。
// 所屬回傳值析構或 vector 重配後其中參考失效。
struct RiverStageOutput {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint16_t> filled_elevation;
    std::vector<std::int32_t> downstream;
    std::vector<std::uint32_t> flow;
    std::vector<std::uint8_t> river_class;
    std::vector<std::uint16_t> moisture;
    std::vector<std::uint8_t> lake;
};

// BiomeStageOutput 是兩張獨立第一命中規則表產生的 terrain／relief 下標。
// build_skeleton 的回傳值擁有它。
// 所屬回傳值析構或 vector 重配後其中參考失效。
struct BiomeStageOutput {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<rules::TerrainId> terrain;
    std::vector<rules::ReliefId> relief;
};

// FeatureStageOutput 是藍噪聲森林與礦脈、綠洲、地標的產物。
// build_skeleton 的回傳值擁有它。
// 所屬回傳值析構或 vector 重配後其中參考失效。
struct FeatureStageOutput {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<rules::FeatureId> feature;
};

[[nodiscard]] ClimateStageOutput generate_climate(const RegionSlowVariables& slow,
                                                  const QuantizedElevation& elevation,
                                                  std::uint64_t stage_seed,
                                                  const ClimateGenerationConfig& config);
[[nodiscard]] RiverStageOutput generate_rivers(const QuantizedElevation& elevation,
                                               const ClimateStageOutput& climate,
                                               std::uint64_t stage_seed,
                                               const RiverGenerationConfig& config);
[[nodiscard]] BiomeStageOutput
generate_biomes(const QuantizedElevation& elevation, const ClimateStageOutput& climate,
                const RiverStageOutput& rivers, const rules::Ruleset& ruleset,
                const RegionDefinitionIds& definitions, std::uint64_t stage_seed,
                const BiomeGenerationConfig& config);
[[nodiscard]] FeatureStageOutput
generate_features(const PlateStageOutput& plates, const QuantizedElevation& elevation,
                  const ClimateStageOutput& climate, const RiverStageOutput& rivers,
                  const BiomeStageOutput& biome, const RegionDefinitionIds& definitions,
                  const rules::Ruleset& ruleset, std::uint64_t stage_seed,
                  const FeatureGenerationConfig& config);

}  // namespace aetheria::worldgen

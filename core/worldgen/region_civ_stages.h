#pragma once

// region_civ_stages.h 收斂城市選址與道路生成兩個階段的型別與函式宣告。

#include "core/rules/ruleset.h"
#include "core/world/region_tiles.h"
#include "core/worldgen/region_climate_stages.h"
#include "core/worldgen/region_config.h"
#include "core/worldgen/region_relief_stages.h"

#include <cstdint>
#include <vector>

namespace aetheria::worldgen {

// RegionDefinitionIds 定義於 region_skeleton.h；此處僅需引用型別供函式宣告的 const 參考使用。
struct RegionDefinitionIds;

// CitySite 是階段 8 選出的 canonical 聚落位置、分數與間距。
// CityStageOutput 擁有所有實例，道路階段只讀取複本。
// 所屬輸出析構或 cities 重配後失效；canonical_id 等於 tile 線性下標。
struct CitySite {
    std::uint32_t canonical_id{};
    world::RegionXY tile;
    std::int32_t score{};
    world::SettlementTier tier{world::SettlementTier::None};
    std::uint16_t minimum_spacing{};

    constexpr bool operator==(const CitySite&) const noexcept = default;
};

// CityStageOutput 是每格定居／瓶頸分數與三級城市清單。
// RegionBuildResult 擁有它，道路階段只在呼叫期間借用。
// 所屬回傳值析構或 vector 重配後其中參考失效。
struct CityStageOutput {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::int32_t> score;
    std::vector<std::uint16_t> bottleneck;
    std::vector<CitySite> cities;
};

// RoadConnection 是 MST 或補環路選出的 canonical 城市對。
// RoadStageOutput 擁有所有實例。
// 所屬輸出析構或 connections 重配後失效。
struct RoadConnection {
    std::uint32_t first_city{};
    std::uint32_t second_city{};
    std::int64_t terrain_cost{};
    bool loop{};

    constexpr bool operator==(const RoadConnection&) const noexcept = default;
};

// RoadStageOutput 是階段 9 的完整雙向 edge、重用次數與路網骨架。
// RegionBuildResult 擁有它，populate 只讀取複本。
// 所屬回傳值析構或 vector 重配後其中參考失效。
struct RoadStageOutput {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<rules::EdgeId> edges;
    std::vector<std::uint16_t> usage;
    std::vector<RoadConnection> connections;
};

[[nodiscard]] CityStageOutput
generate_cities(const QuantizedElevation& elevation, const ClimateStageOutput& climate,
                const RiverStageOutput& rivers, const BiomeStageOutput& biome,
                const FeatureStageOutput& features, const rules::Ruleset& ruleset,
                std::uint64_t stage_seed, const CityGenerationConfig& config);
[[nodiscard]] RoadStageOutput
generate_roads(const QuantizedElevation& elevation, const ClimateStageOutput& climate,
               const RiverStageOutput& rivers, const BiomeStageOutput& biome,
               const FeatureStageOutput& features, const CityStageOutput& cities,
               const RegionDefinitionIds& definitions, const rules::Ruleset& ruleset,
               std::uint64_t stage_seed, const RoadGenerationConfig& config,
               bool canonicalize_city_order = true);

}  // namespace aetheria::worldgen

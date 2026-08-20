#pragma once

// region_late_stages.h 定義 Region 階段 11 出境點與階段 12 勢力起始的純函式介面。

#include "core/worldgen/influence_spread.h"
#include "core/worldgen/region_civ_stages.h"

#include <cstdint>
#include <vector>

namespace aetheria::worldgen {

// PortalStageOutput 是階段 11 的完整聚落、道路與稀疏 portal 產物。
// RegionBuildResult 擁有值，階段 12 與 populate 只借用 const 參考。
struct PortalStageOutput {
    std::uint32_t width{};
    std::uint32_t height{};
    CityStageOutput cities;
    std::vector<rules::EdgeId> edges;
    std::vector<world::RegionPortal> portals;
};

// FactionStageOutput 是階段 12 的首都配發與逐格 owner 產物。
// RegionBuildResult 擁有值，populate 只借用 const 參考。
struct FactionStageOutput {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<InfluenceCapital> capitals;
    std::vector<world::FactionId> owner;
};

[[nodiscard]] PortalStageOutput
generate_portals(const QuantizedElevation& elevation, const ClimateStageOutput& climate,
                 const RiverStageOutput& rivers, const BiomeStageOutput& biome,
                 const HistoryStageOutput& history, const CityStageOutput& cities,
                 const RoadStageOutput& roads, const RegionDefinitionIds& definitions,
                 const rules::Ruleset& ruleset, std::uint32_t region_id,
                 std::uint64_t stage_seed, const PortalGenerationConfig& config);

[[nodiscard]] FactionStageOutput
generate_factions(const QuantizedElevation& elevation, const ClimateStageOutput& climate,
                  const RiverStageOutput& rivers, const BiomeStageOutput& biome,
                  const HistoryStageOutput& history, const PortalStageOutput& portals,
                  const RegionDefinitionIds& definitions, const rules::Ruleset& ruleset,
                  std::uint64_t stage_seed, const FactionGenerationConfig& config);

}  // namespace aetheria::worldgen

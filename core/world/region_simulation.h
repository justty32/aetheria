#pragma once

// region_simulation.h 定義 Region 第 5 階段的低解析度快變數近似公式。

#include "core/world/region_tiles.h"

#include <cstddef>
#include <cstdint>

namespace aetheria::world {

struct RegionFormulaResult {
    PopulationReduction::Value population{};
    DevelopmentLevelReduction::Value development_level{};
};

struct RegionSimulationReport {
    std::size_t formula_execution_count{};
    std::size_t live_site_skip_count{};
};

[[nodiscard]] RegionFormulaResult region_formula(SettlementTier settlement);

class RegionSimulation {
public:
    [[nodiscard]] static RegionSimulationReport advance_xun(RegionTiles& tiles);
};

}  // namespace aetheria::world

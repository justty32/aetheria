#include "core/world/region_simulation.h"

#include <stdexcept>

namespace aetheria::world {

RegionFormulaResult region_formula(SettlementTier settlement) {
    switch (settlement) {
    case SettlementTier::None:
        return {};
    case SettlementTier::Village:
    case SettlementTier::Town:
    case SettlementTier::City:
        return {.population = 100, .development_level = 1};
    }
    throw std::runtime_error{"Region 近似公式遇到無效 SettlementTier"};
}

RegionSimulationReport RegionSimulation::advance_xun(RegionTiles& tiles) {
    if (!tiles.valid_layout()) {
        throw std::runtime_error{"Region 世界模擬不能處理版面無效的 RegionTiles"};
    }
    auto& population =
        std::get<ReductionField<PopulationReduction>>(tiles.reduction_fields_.fields).values;
    auto& development =
        std::get<ReductionField<DevelopmentLevelReduction>>(tiles.reduction_fields_.fields).values;
    RegionSimulationReport report;
    for (std::size_t index = 0; index < tiles.tile_count(); ++index) {
        if (tiles.site[index].has_live_site) {
            ++report.live_site_skip_count;
            continue;
        }
        const auto approximation = region_formula(tiles.settlement[index]);
        population[index] = approximation.population;
        development[index] = approximation.development_level;
        ++report.formula_execution_count;
    }
    return report;
}

}  // namespace aetheria::world

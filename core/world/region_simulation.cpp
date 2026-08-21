#include "core/world/region_simulation.h"

#include <algorithm>
#include <limits>
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

RegionFormulaResult region_formula(SettlementTier settlement,
                                   PopulationReduction::Value population,
                                   FoodStockReduction::Value food_stock,
                                   ProductionStockReduction::Value production_stock) {
    const auto baseline = region_formula(settlement);
    if (settlement == SettlementTier::None) {
        return baseline;
    }
    const auto current = population == 0 ? baseline.population : population;
    // 預設完整城建：每旬 500 bp × 85 滿意度 = 425 bp；Region 用最近整數近似。
    constexpr std::uint64_t growth_basis_points = 425;
    const auto growth = std::max<PopulationReduction::Value>(
        1U, static_cast<PopulationReduction::Value>(
                (static_cast<std::uint64_t>(current) * growth_basis_points + 5'000U) / 10'000U));
    if (current > std::numeric_limits<PopulationReduction::Value>::max() - growth) {
        throw std::overflow_error{"Region 人口近似推進溢位"};
    }
    return {.population = static_cast<PopulationReduction::Value>(current + growth),
            .development_level = baseline.development_level,
            .food_stock = food_stock,
            .production_stock = production_stock};
}

RegionSimulationReport RegionSimulation::advance_xun(RegionTiles& tiles) {
    if (!tiles.valid_layout()) {
        throw std::runtime_error{"Region 世界模擬不能處理版面無效的 RegionTiles"};
    }
    auto& population =
        std::get<ReductionField<PopulationReduction>>(tiles.reduction_fields_.fields).values;
    auto& development =
        std::get<ReductionField<DevelopmentLevelReduction>>(tiles.reduction_fields_.fields).values;
    auto& food =
        std::get<ReductionField<FoodStockReduction>>(tiles.reduction_fields_.fields).values;
    auto& production =
        std::get<ReductionField<ProductionStockReduction>>(tiles.reduction_fields_.fields).values;
    RegionSimulationReport report;
    for (std::size_t index = 0; index < tiles.tile_count(); ++index) {
        if (tiles.site[index].has_live_site) {
            ++report.live_site_skip_count;
            continue;
        }
        const auto approximation = region_formula(tiles.settlement[index], population[index],
                                                  food[index], production[index]);
        population[index] = approximation.population;
        development[index] = approximation.development_level;
        food[index] = approximation.food_stock;
        production[index] = approximation.production_stock;
        ++report.formula_execution_count;
    }
    return report;
}

}  // namespace aetheria::world

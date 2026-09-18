#include "core/site/site_reduction.h"

#include "core/site/site_build_loop.h"
#include "core/zone/zone_key.h"

#include <array>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace aetheria::site {
namespace {

constexpr std::array<BuildingReductionWeight, 6> kBuildingWeights{{
    {100, 1},  // SettlementHall
    {0, 2},    // Residence
    {0, 1},    // Farm
    {0, 2},    // Mine
    {0, 2},    // Workshop
    {0, 1},    // CivicSquare
}};

[[nodiscard]] std::uint32_t population_contribution(const PersistentBuilding& building) {
    const auto base = building_reduction_weight(building.type).population;
    switch (building.state) {
    case BuildingState::Active:
        return base;
    case BuildingState::Idle:
        return base * 3U / 4U;
    case BuildingState::Derelict:
        return base / 4U;
    case BuildingState::Ruined:
        return 0;
    }
    throw std::runtime_error{"歸約量表遇到無效建築狀態"};
}

[[nodiscard]] std::uint16_t development_contribution(const PersistentBuilding& building) {
    const auto base = building_reduction_weight(building.type).development;
    switch (building.state) {
    case BuildingState::Active:
    case BuildingState::Idle:
        return base;
    case BuildingState::Derelict:
    case BuildingState::Ruined:
        return 0;
    }
    throw std::runtime_error{"歸約量表遇到無效建築狀態"};
}

template <typename Row> [[nodiscard]] typename Row::Value measure(const SiteLayers& layers) {
    typename Row::Value result{};
    for (const auto& building : layers.persistent.buildings) {
        const auto contribution = [&]() {
            if constexpr (std::is_same_v<Row, world::PopulationReduction>) {
                return population_contribution(building);
            } else if constexpr (std::is_same_v<Row, world::DevelopmentLevelReduction>) {
                return development_contribution(building);
            } else {
                static_assert(std::is_same_v<Row, world::FoodStockReduction> ||
                              std::is_same_v<Row, world::ProductionStockReduction> ||
                              std::is_same_v<Row, world::OrderReduction>);
                return typename Row::Value{};
            }
        }();
        if (contribution > std::numeric_limits<typename Row::Value>::max() - result) {
            throw std::overflow_error{"Site 歸約量超出 Region 欄位容量"};
        }
        result = static_cast<typename Row::Value>(result + contribution);
    }
    return result;
}

}  // namespace

const BuildingReductionWeight& building_reduction_weight(BuildingType type) {
    const auto index = static_cast<std::size_t>(type);
    if (index >= kBuildingWeights.size()) {
        throw std::runtime_error{"歸約量表遇到未登記的建築型別"};
    }
    return kBuildingWeights[index];
}

std::optional<world::OrderReduction::Value>
measure_site_order(const SitePersistentLayer& persistent) noexcept {
    if (!persistent.order.has_value()) {
        return std::nullopt;
    }
    const auto coverage = static_cast<std::uint32_t>(persistent.order->garrison_coverage) +
                          persistent.order->patrol_coverage;
    const auto pressure = static_cast<std::uint32_t>(persistent.order->bandit_pressure) +
                          persistent.order->refugee_pressure;
    return static_cast<world::OrderReduction::Value>(coverage > pressure ? coverage - pressure
                                                                         : 0U);
}

world::RegionTileDelta ReductionTable::reduce(const SiteLayers& layers) {
    if (!valid_persistent_layer(layers.persistent)) {
        throw std::runtime_error{"不能歸約無效的 SitePersistentLayer"};
    }
    world::RegionTileDelta result;
    std::apply(
        [&](auto&... value) {
            (([&] {
                 using Row = typename std::remove_cvref_t<decltype(value)>::RowType;
                 if constexpr (std::is_same_v<Row, world::OrderReduction>) {
                     value.value = measure_site_order(layers.persistent);
                 } else {
                     value.value = measure<Row>(layers);
                 }
             }()),
             ...);
        },
        result.values_);
    return result;
}

}  // namespace aetheria::site

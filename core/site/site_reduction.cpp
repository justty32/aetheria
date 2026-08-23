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

void validate_site_identity(const zone::Zone& live_site, world::RegionXY coordinate) {
    if (zone::level_of(live_site.key) != zone::ZoneLevel::Site ||
        coordinate.x < 0 || coordinate.y < 0 ||
        zone::site_x_of(live_site.key) != static_cast<std::uint16_t>(coordinate.x) ||
        zone::site_y_of(live_site.key) != static_cast<std::uint16_t>(coordinate.y)) {
        throw std::invalid_argument{"Site 歸約的 ZoneKey 與 RegionXY 不一致"};
    }
    if (live_site.lod == zone::LodLevel::Absent) {
        throw std::logic_error{"不能歸約 L_ABSENT Site"};
    }
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

world::RegionTileDelta ReductionTable::reduce(const zone::Zone& site,
                                              const rules::Ruleset& ruleset) {
    const auto* payload = std::get_if<zone::SitePayload>(&site.payload);
    if (payload == nullptr) {
        throw std::invalid_argument{"ReductionTable::reduce(zone) 只接受 SitePayload"};
    }
    auto result = reduce(payload->layers);
    const auto states = site.reg.view<const CityBuildState>();
    if (states.empty()) {
        return result;
    }
    if (states.size() != 1U) {
        throw std::logic_error{"Site 歸約遇到多個 CityBuildState"};
    }
    const auto& state = states.get<const CityBuildState>(*states.begin());
    auto& development_observation =
        std::get<world::ReductionValue<world::DevelopmentLevelReduction>>(result.values_).value;
    if (!development_observation.has_value()) {
        throw std::logic_error{"城市 Site 的建設歸約列不得缺席"};
    }
    auto development = *development_observation;
    for (const auto& building : state.buildings) {
        const auto id = ruleset.find_city_building(building.definition_id);
        const auto* definition = id.has_value() ? ruleset.city_building(*id) : nullptr;
        if (definition == nullptr) {
            throw std::runtime_error{"歸約量表遇到未登記的城建 def：" + building.definition_id};
        }
        const auto contribution =
            building_reduction_weight(definition->persistent_type).development;
        if (contribution >
            std::numeric_limits<world::DevelopmentLevelReduction::Value>::max() - development) {
            throw std::overflow_error{"Site 歸約量超出 Region 欄位容量"};
        }
        development =
            static_cast<world::DevelopmentLevelReduction::Value>(development + contribution);
    }
    development_observation = development;
    const auto& economy = state.economy;
    std::get<world::ReductionValue<world::PopulationReduction>>(result.values_).value =
        economy.population;
    std::get<world::ReductionValue<world::FoodStockReduction>>(result.values_).value =
        economy.food_stock;
    std::get<world::ReductionValue<world::ProductionStockReduction>>(result.values_).value =
        economy.production_stock;
    return result;
}

void ReductionTable::apply(world::RegionTiles& tiles, world::RegionXY coordinate,
                           const world::RegionTileDelta& delta) {
    if (!tiles.valid_layout()) {
        throw std::runtime_error{"不能把歸約量套用到版面無效的 RegionTiles"};
    }
    const auto index = tiles.index_of(coordinate);
    static_cast<void>(delta.apply_to(tiles.reduction_fields_.fields, index));
}

void reduce_live_site_xun(world::RegionTiles& tiles, world::RegionXY coordinate,
                          const zone::Zone& live_site, const rules::Ruleset& ruleset) {
    validate_site_identity(live_site, coordinate);
    const auto index = tiles.index_of(coordinate);
    if (!tiles.site.at(index).has_live_site) {
        throw std::logic_error{"Region tile 未標記 has_live_site，拒絕歸約"};
    }
    const auto* payload = std::get_if<zone::SitePayload>(&live_site.payload);
    if (payload == nullptr) {
        throw std::invalid_argument{"Site Zone 缺少 SitePayload"};
    }
    ReductionTable::apply(tiles, coordinate, ReductionTable::reduce(live_site, ruleset));
}

}  // namespace aetheria::site

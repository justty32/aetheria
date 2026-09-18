#include "core/site/site_reduction.h"

#include "core/site/site_build_loop.h"
#include "core/zone/zone_key.h"

#include <array>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace aetheria::site {
namespace {

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

#include "core/site/site_build_loop.h"

#include "core/site/site_build_loop_detail.h"
#include "core/site/site_lifecycle.h"
#include "core/site/site_reduction.h"
#include "core/zone/zone_key.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace aetheria::site {

const rules::CityBuildingDef& build_detail::require_definition(
    const rules::Ruleset& ruleset, std::string_view id) {
    const auto found = ruleset.find_city_building(id);
    const auto* definition = found.has_value() ? ruleset.city_building(*found) : nullptr;
    if (definition == nullptr) {
        throw std::invalid_argument{"城建引用不存在的 building def：" + std::string{id}};
    }
    return *definition;
}

namespace {

[[nodiscard]] bool rectangles_overlap(SiteXY a, const rules::CityBuildingDef& a_def, SiteXY b,
                                      const rules::CityBuildingDef& b_def) noexcept {
    return a.x < static_cast<std::uint32_t>(b.x) + b_def.width &&
           b.x < static_cast<std::uint32_t>(a.x) + a_def.width &&
           a.y < static_cast<std::uint32_t>(b.y) + b_def.height &&
           b.y < static_cast<std::uint32_t>(a.y) + a_def.height;
}

void require_placement(const SiteLayers& layers, const CityBuildState& state, SiteXY origin,
                       const rules::CityBuildingDef& definition,
                       const rules::Ruleset& ruleset) {
    const auto x_end = static_cast<std::uint32_t>(origin.x) + definition.width;
    const auto y_end = static_cast<std::uint32_t>(origin.y) + definition.height;
    if (x_end > kSiteWidth || y_end > kSiteHeight) {
        throw std::invalid_argument{"城建位置超出 Site 邊界"};
    }
    for (std::uint32_t y = origin.y; y < y_end; ++y) {
        for (std::uint32_t x = origin.x; x < x_end; ++x) {
            const auto index = static_cast<std::size_t>(y) * kSiteWidth + x;
            if (!layers.procedural.skeleton.valid_layout() ||
                layers.procedural.skeleton.buildable[index] == 0 ||
                layers.procedural.skeleton.roads[index] != 0) {
                throw std::invalid_argument{"城建位置不是可建且無道路的格子"};
            }
        }
    }
    const auto occupied = [&](std::string_view id, SiteXY other) {
        return rectangles_overlap(origin, definition, other,
                                  build_detail::require_definition(ruleset, id));
    };
    if (std::ranges::any_of(state.buildings, [&](const CityBuilding& building) {
            return occupied(building.definition_id, building.origin);
        }) ||
        std::ranges::any_of(state.pending, [&](const PendingConstruction& building) {
            return occupied(building.definition_id, building.origin);
        })) {
        throw std::invalid_argument{"城建位置與既有建築或工地重疊"};
    }
}

world::RegionTiles& require_region_layer(zone::Zone& region, std::int8_t z) {
    auto* payload = std::get_if<zone::RegionPayload>(&region.payload);
    if (payload == nullptr || !payload->layers.contains(z)) {
        throw std::invalid_argument{"SiteTurnPipeline 找不到 Region layer"};
    }
    return payload->layers.at(z);
}

}  // namespace

const CityBuildState& city_build_state(const zone::Zone& site) {
    const auto states = site.reg.view<const CityBuildState>();
    if (states.size() != 1U) {
        throw std::logic_error{"L_FULL 城市 Site 必須恰有一個 CityBuildState"};
    }
    return states.get<const CityBuildState>(*states.begin());
}

CityBuildState& city_build_state(zone::Zone& site) {
    auto states = site.reg.view<CityBuildState>();
    if (states.size() != 1U) {
        throw std::logic_error{"L_FULL 城市 Site 必須恰有一個 CityBuildState"};
    }
    return states.get<CityBuildState>(*states.begin());
}

void enter_full_site(zone::Zone& site, world::RegionTiles& tiles,
                     world::RegionXY coordinate) {
    if (zone::level_of(site.key) != zone::ZoneLevel::Site ||
        !std::holds_alternative<zone::SitePayload>(site.payload) ||
        (site.lod != zone::LodLevel::Coarse && site.lod != zone::LodLevel::Full)) {
        throw std::invalid_argument{"enter_full_site 只接受已具現化的 L_COARSE/L_FULL Site"};
    }
    const auto index = tiles.index_of(coordinate);
    if (!tiles.site.at(index).has_live_site) {
        throw std::logic_error{"enter_full_site 要求 Region tile 已標記 live"};
    }
    auto states = site.reg.view<CityBuildState>();
    if (states.empty()) {
        const auto metas = site.reg.view<zone::ZoneMeta>();
        if (metas.empty()) {
            throw std::logic_error{"Site 缺少 ZoneMeta，不能建立 CityBuildState"};
        }
        CityBuildState initial;
        const auto region_population =
            tiles.reduction_value<world::PopulationReduction>(coordinate);
        const auto& layers = std::get<zone::SitePayload>(site.payload).layers;
        initial.economy.population =
            region_population != 0
                ? region_population
                : ReductionTable::reduce(layers).value<world::PopulationReduction>();
        site.reg.emplace<CityBuildState>(*metas.begin(), std::move(initial));
    } else if (states.size() != 1U) {
        throw std::logic_error{"Site 含多個 CityBuildState"};
    }
    site.lod = zone::LodLevel::Full;
    tiles.site.at(index).lod = zone::LodLevel::Full;
}

void start_construction(zone::Zone& site, std::string_view definition_id, SiteXY origin,
                        const rules::Ruleset& ruleset) {
    if (site.lod != zone::LodLevel::Full) {
        throw std::logic_error{"只有 L_FULL Site 能開始逐格建造"};
    }
    auto& state = city_build_state(site);
    const auto& definition = build_detail::require_definition(ruleset, definition_id);
    const auto& layers = std::get<zone::SitePayload>(site.payload).layers;
    require_placement(layers, state, origin, definition, ruleset);
    state.pending.push_back(
        {definition.id, origin, definition.construction_hours});
}

SiteAdvanceReport SiteTurnPipeline::advance_hours(zone::Zone& site, zone::Zone& region,
                                                  std::int8_t region_z,
                                                  world::RegionXY coordinate,
                                                  std::uint32_t hours) const {
    if (site.lod != zone::LodLevel::Full) {
        throw std::logic_error{"SiteTurnPipeline 只推進 L_FULL Site"};
    }
    auto& tiles = require_region_layer(region, region_z);
    SiteAdvanceReport report;
    for (std::uint32_t hour = 0; hour < hours; ++hour) {
        auto& layers = std::get<zone::SitePayload>(site.payload).layers;
        const auto aging =
            advance_persistent_objects(layers.persistent, split_site_vars(tiles, coordinate).fast,
                                       time::kHour);
        report.persistent_object_advances += aging.persistent_objects_advanced;
        report.aging_transitions += aging.aging_transitions;
        report.aging_cap_hit = report.aging_cap_hit || aging.aging_cap_hit;
        auto& state = city_build_state(site);
        const auto completed = build_detail::simulate_hour(state, ruleset_, report);
        ++report.hours_advanced;
        report.constructions_completed += completed;
        if (completed != 0) {
            reduce_live_site_xun(tiles, coordinate, site);
            ++report.completion_reductions;
        }
        if (state.economy.hours_into_xun == 0) {
            region_turn_.advance_xun(region, {}, [&](zone::Zone& reducing_region) {
                auto& reducing_tiles = require_region_layer(reducing_region, region_z);
                reduce_live_site_xun(reducing_tiles, coordinate, site);
            });
            ++report.xun_boundaries;
        }
    }
    return report;
}

}  // namespace aetheria::site

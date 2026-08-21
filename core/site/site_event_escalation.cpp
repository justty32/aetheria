#include "core/site/site_event_escalation.h"

#include "core/site/site_reduction.h"
#include "core/zone/zone_key.h"

#include <algorithm>
#include <stdexcept>

namespace aetheria::site {
namespace {

[[nodiscard]] SiteLayers& require_matching_live_site(world::RegionTiles& tiles,
                                                     world::RegionXY coordinate,
                                                     zone::Zone& live_site) {
    if (!tiles.valid_layout()) {
        throw std::runtime_error{"Site 事件不能寫入版面無效的 RegionTiles"};
    }
    const auto index = tiles.index_of(coordinate);
    if (!tiles.site.at(index).has_live_site || live_site.lod == zone::LodLevel::Absent) {
        throw std::logic_error{"Site 事件升級要求已載入的 live Site"};
    }
    if (zone::level_of(live_site.key) != zone::ZoneLevel::Site || coordinate.x < 0 ||
        coordinate.y < 0 ||
        zone::site_x_of(live_site.key) != static_cast<std::uint16_t>(coordinate.x) ||
        zone::site_y_of(live_site.key) != static_cast<std::uint16_t>(coordinate.y)) {
        throw std::invalid_argument{"Site 事件的 ZoneKey 與 RegionXY 不一致"};
    }
    auto* payload = std::get_if<zone::SitePayload>(&live_site.payload);
    if (payload == nullptr) {
        throw std::invalid_argument{"Site 事件的 Zone 缺少 SitePayload"};
    }
    return payload->layers;
}

}  // namespace

bool apply_site_building_state_event(world::RegionTiles& tiles, world::RegionXY coordinate,
                                     zone::Zone& live_site,
                                     const SiteBuildingStateEvent& event) {
    if (event.significance > world::Significance::World) {
        throw std::invalid_argument{"Site 事件含無效的重要性等級"};
    }
    if (event.new_state > BuildingState::Ruined) {
        throw std::invalid_argument{"Site 建築狀態事件含無效狀態"};
    }
    auto& layers = require_matching_live_site(tiles, coordinate, live_site);
    const auto building = std::ranges::find(layers.persistent.buildings, event.building,
                                            &PersistentBuilding::tile);
    if (building == layers.persistent.buildings.end()) {
        throw std::invalid_argument{"Site 建築狀態事件找不到目標持久建築"};
    }
    const bool escalated = world::reaches(event.significance, world::Significance::Region);
    const auto old_state = building->state;
    building->state = event.new_state;
    try {
        if (escalated) {
            ReductionTable::apply(tiles, coordinate, ReductionTable::reduce(layers));
        }
    } catch (...) {
        building->state = old_state;
        throw;
    }
    return escalated;
}

}  // namespace aetheria::site

#pragma once

// site_event_escalation.h 是 Site 事件跨到 Region 的最小即時界面，不保存或聚合事件。

#include "core/site/site_projection.h"
#include "core/world/region_tiles.h"
#include "core/world/significance.h"
#include "core/zone/zone.h"

namespace aetheria::site {

// SiteBuildingStateEvent 表示一棟持久建築已發生離散狀態變化。
// significance 沿用實體的重要性型別；事件與呼叫端共同擁有值。
struct SiteBuildingStateEvent {
    world::Significance significance{world::Significance::Site};
    SiteXY building;
    BuildingState new_state{BuildingState::Active};
};

// 事件一律先落到 Site 持久來源；達 Region 級時立即同步 Region 歸約快變數。
// 回傳 true 表示本次已升級，false 表示只等待該旬正常歸約。
[[nodiscard]] bool apply_site_building_state_event(
    world::RegionTiles& tiles, world::RegionXY coordinate, zone::Zone& live_site,
    const SiteBuildingStateEvent& event);

}  // namespace aetheria::site

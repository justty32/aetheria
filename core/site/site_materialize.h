#pragma once

// site_materialize.h 把單一 Region tile 確定性展開成 L_COARSE Site zone，並管理其收回／重算。

#include "core/rules/ruleset.h"
#include "core/site/site_lifecycle.h"
#include "core/world/region_tiles.h"
#include "core/zone/zone.h"
#include "core/zone/zone_manager.h"

#include <cstdint>

namespace aetheria::site {

[[nodiscard]] zone::Zone materialize_site_zone(world::RegionTiles& region_tiles,
                                               world::RegionXY coordinate, std::uint64_t world_seed,
                                               std::uint32_t region_id,
                                               const rules::Ruleset& ruleset);

// rematerialize_site_zone 只接受 L_ABSENT（未載入）的 Site，從 store 冷載持久層，
// 再以當下 Region tile 重算程序層。回傳 handle 指向 L_COARSE Site。
[[nodiscard]] zone::ZoneHandle rematerialize_site_zone(
    zone::ZoneManager& manager, world::RegionTiles& region_tiles,
    world::RegionXY coordinate, std::uint64_t world_seed, std::uint32_t region_id,
    const rules::Ruleset& ruleset);

// 有 SiteDigest 的真正重載入口；now 用全局秒時鐘驅動一次閉式補算。
[[nodiscard]] zone::ZoneHandle rematerialize_site_zone(
    zone::ZoneManager& manager, world::RegionTiles& region_tiles,
    world::RegionXY coordinate, std::uint64_t world_seed, std::uint32_t region_id,
    time::Tick now, const rules::Ruleset& ruleset, SiteCatchUpReport* report = nullptr);

// collapse_site_zone 先強制歸約，再把 L_COARSE Site 寫盤並移出 manager。
void collapse_site_zone(zone::ZoneManager& manager, zone::ZoneHandle handle,
                        world::RegionTiles& region_tiles, world::RegionXY coordinate);

// M4 卸載入口：以 digest 取代 live 城建 component，記錄卸載時鐘後進入 L_ABSENT。
void unload_site_zone(zone::ZoneManager& manager, zone::ZoneHandle handle,
                      world::RegionTiles& region_tiles, world::RegionXY coordinate,
                      std::uint64_t world_seed, std::uint32_t region_id, time::Tick now);

}  // namespace aetheria::site

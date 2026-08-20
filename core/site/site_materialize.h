#pragma once

// site_materialize.h 把單一 Region tile 確定性展開成 L_COARSE Site zone，並管理其收回／重算。

#include "core/rules/ruleset.h"
#include "core/world/region_tiles.h"
#include "core/zone/zone.h"
#include "core/zone/zone_manager.h"

#include <cstdint>

namespace aetheria::site {

[[nodiscard]] zone::Zone materialize_site_zone(const world::RegionTiles& region_tiles,
                                               world::RegionXY coordinate, std::uint64_t world_seed,
                                               std::uint32_t region_id,
                                               const rules::Ruleset& ruleset);

// rematerialize_site_zone 只接受 L_ABSENT（未載入）的 Site，從 store 冷載持久層，
// 再以當下 Region tile 重算程序層。回傳 handle 指向 L_COARSE Site。
[[nodiscard]] zone::ZoneHandle rematerialize_site_zone(
    zone::ZoneManager& manager, const world::RegionTiles& region_tiles,
    world::RegionXY coordinate, std::uint64_t world_seed, std::uint32_t region_id,
    const rules::Ruleset& ruleset);

// collapse_site_zone 把已載入的 L_COARSE Site 寫盤並移出 ZoneManager，成為 L_ABSENT。
void collapse_site_zone(zone::ZoneManager& manager, zone::ZoneHandle handle);

}  // namespace aetheria::site

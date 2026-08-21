#pragma once

// site_reduction.h 是 L2→L1 唯一的連續量歸約界面。

#include "core/site/site_projection.h"
#include "core/world/reduction_schema.h"
#include "core/world/region_tiles.h"
#include "core/zone/zone.h"

namespace aetheria::site {

// ReductionTable 是 Region 歸約欄位唯一的 Site-side 寫入者。
// row 清單同時決定 delta 與 Region storage；呼叫端拿不到任意 setter。
class ReductionTable {
public:
    [[nodiscard]] static world::RegionTileDelta reduce(const SiteLayers& layers);
    [[nodiscard]] static world::RegionTileDelta reduce(const zone::Zone& site);
    static void apply(world::RegionTiles& tiles, world::RegionXY coordinate,
                      const world::RegionTileDelta& delta);
};

// 每旬對仍在記憶體中的 Site 呼叫一次；亦由 collapse 路徑在卸載前強制呼叫。
void reduce_live_site_xun(world::RegionTiles& tiles, world::RegionXY coordinate,
                          const zone::Zone& live_site);

}  // namespace aetheria::site

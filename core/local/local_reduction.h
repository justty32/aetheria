#pragma once

// local_reduction.h：L3→L2 唯一的連續量歸約界面。

#include <cstddef>

#include "core/local/local_reduction_schema.h"
#include "core/site/site_projection.h"
#include "core/zone/zone.h"

namespace aetheria::local {

class ReductionTable {
public:
    [[nodiscard]] static site::LocalTileDelta reduce(const LocalReductionState& state);
    [[nodiscard]] static site::LocalTileDelta reduce(const zone::Zone& local);
    [[nodiscard]] static std::size_t apply(site::SiteLayers& site_layers,
                                           site::SiteXY coordinate,
                                           const site::LocalTileDelta& delta);
};

// 每旬與卸載路徑共用此入口；key、LOD 或 payload 不符時拒絕歸約。
[[nodiscard]] std::size_t reduce_live_local(site::SiteLayers& site_layers,
                                            site::SiteXY coordinate,
                                            const zone::Zone& live_local);

}  // namespace aetheria::local

#pragma once

// 具現化各階段共用的程序層準備與 digest 恢復介面。
#include "core/site/site_materialize.h"
#include "core/site/site_wilderness.h"

#include <optional>

namespace aetheria::site::detail {

struct PreparedSite {
    SiteProceduralLayer procedural;
    std::optional<WildernessSite> wilderness;
};

[[nodiscard]] zone::ZoneKey site_key_for(std::uint32_t region_id,
                                         world::RegionXY coordinate);

[[nodiscard]] PreparedSite prepare_site(const world::RegionTiles& region_tiles,
                                        world::RegionXY coordinate, std::uint64_t world_seed,
                                        std::uint32_t region_id, const SiteProjectionVars& vars,
                                        const rules::Ruleset& ruleset);

void restore_loaded_site(zone::Zone& loaded, PreparedSite prepared,
                         const SiteProjectionVars& vars, std::uint64_t expected_seed,
                         time::Tick now, const rules::Ruleset& ruleset,
                         SiteCatchUpReport* report);

}  // namespace aetheria::site::detail

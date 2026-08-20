#pragma once

// site_materialize.h 把單一 Region tile 確定性展開成 L_COARSE Site zone。

#include "core/rules/ruleset.h"
#include "core/world/region_tiles.h"
#include "core/zone/zone.h"

#include <cstdint>

namespace aetheria::site {

[[nodiscard]] zone::Zone materialize_site_zone(const world::RegionTiles& region_tiles,
                                               world::RegionXY coordinate, std::uint64_t world_seed,
                                               std::uint32_t region_id,
                                               const rules::Ruleset& ruleset);

}  // namespace aetheria::site

#pragma once

// portal_candidates.h 是階段 11 各 WorldGraph 通道型別的落點解析內部介面。

#include "core/worldgen/region_late_stages.h"

#include <cstddef>

namespace aetheria::worldgen::detail {

[[nodiscard]] std::size_t resolve_sea_portal(world::RegionTiles& tiles,
                                             CityStageOutput& cities,
                                             const rules::Ruleset& ruleset);
[[nodiscard]] std::size_t resolve_boundary_portal(const world::RegionTiles& tiles,
                                                  const rules::Ruleset& ruleset,
                                                  bool underground);
[[nodiscard]] std::size_t
resolve_teleport_portal(const world::RegionTiles& tiles,
                        const rules::WorldGraphConnection& connection, bool endpoint_a,
                        const rules::Ruleset& ruleset);

}  // namespace aetheria::worldgen::detail

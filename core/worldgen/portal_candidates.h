#pragma once

// portal_candidates.h 是階段 11 各 WorldGraph 通道型別的落點解析內部介面。

#include "core/worldgen/region_late_stages.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace aetheria::worldgen::detail {

[[nodiscard]] std::size_t resolve_sea_portal(world::RegionTiles& tiles, CityStageOutput& cities,
                                             const rules::Ruleset& ruleset,
                                             std::span<const std::uint8_t> occupied);
[[nodiscard]] std::size_t resolve_boundary_portal(const world::RegionTiles& tiles,
                                                  const rules::Ruleset& ruleset, bool underground,
                                                  std::span<const std::uint8_t> occupied);
[[nodiscard]] std::size_t resolve_teleport_portal(const world::RegionTiles& tiles,
                                                  const rules::WorldGraphConnection& connection,
                                                  bool endpoint_a, const rules::Ruleset& ruleset,
                                                  std::span<const std::uint8_t> occupied);

}  // namespace aetheria::worldgen::detail

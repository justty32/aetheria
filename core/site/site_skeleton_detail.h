#pragma once

// 城區骨架 S1～S4 的內部階段介面；只供 core/site 實作使用。

#include "core/site/site_projection.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace aetheria::site::detail {

inline constexpr std::size_t kDirections = 4;

[[nodiscard]] constexpr std::size_t tile_index(std::uint16_t x,
                                               std::uint16_t y) noexcept {
  return static_cast<std::size_t>(y) * kSiteWidth + x;
}

[[nodiscard]] constexpr std::size_t tile_index(SiteXY tile) noexcept {
  return tile_index(tile.x, tile.y);
}

[[nodiscard]] constexpr std::size_t side_index(SiteBoundarySide side) noexcept {
  return static_cast<std::size_t>(side);
}

[[nodiscard]] std::uint16_t boundary_position(std::uint64_t site_seed,
                                              SiteBoundarySide side) noexcept;
[[nodiscard]] std::uint16_t local_slope(const SiteSkeleton &skeleton,
                                        std::uint16_t x,
                                        std::uint16_t y) noexcept;
[[nodiscard]] SiteXY choose_city_center(const SiteSkeleton &skeleton);

void generate_site_terrain(SiteSkeleton &skeleton, const SiteSlowVars &slow,
                           std::uint64_t site_seed,
                           const rules::Ruleset &ruleset);
void generate_site_roads(SiteSkeleton &skeleton, const SiteSlowVars &slow,
                         std::uint64_t site_seed,
                         const rules::Ruleset &ruleset);
void generate_site_blocks(SiteSkeleton &skeleton, std::uint64_t site_seed,
                          const rules::Ruleset &ruleset);
void mark_site_buildable(SiteSkeleton &skeleton, const rules::Ruleset &ruleset);

} // namespace aetheria::site::detail

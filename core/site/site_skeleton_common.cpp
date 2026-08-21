#include "core/site/site_skeleton_detail.h"

#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace aetheria::site::detail {

std::uint16_t boundary_position(std::uint64_t site_seed,
                                SiteBoundarySide side) noexcept {
  constexpr std::uint64_t kBoundarySalt = UINT64_C(0x7DCC8B1A6E23F495);
  constexpr std::uint16_t kMargin = 8;
  constexpr std::uint16_t kSpan = kSiteWidth - kMargin * 2U;
  const auto sample =
      worldgen::splitmix64(site_seed ^ kBoundarySalt ^ side_index(side));
  return static_cast<std::uint16_t>(kMargin + sample % kSpan);
}

std::uint16_t local_slope(const SiteSkeleton &skeleton, std::uint16_t x,
                          std::uint16_t y) noexcept {
  const auto center = skeleton.elevation[tile_index(x, y)];
  std::uint16_t result{};
  constexpr std::array<std::array<std::int16_t, 2>, 4> offsets{
      {{{0, -1}}, {{1, 0}}, {{0, 1}}, {{-1, 0}}}};
  for (const auto &offset : offsets) {
    const auto nx = static_cast<std::int32_t>(x) + offset[0];
    const auto ny = static_cast<std::int32_t>(y) + offset[1];
    if (nx < 0 || ny < 0 || nx >= static_cast<std::int32_t>(kSiteWidth) ||
        ny >= static_cast<std::int32_t>(kSiteHeight)) {
      continue;
    }
    const auto neighbor = skeleton.elevation[tile_index(
        static_cast<std::uint16_t>(nx), static_cast<std::uint16_t>(ny))];
    const auto difference = static_cast<std::uint16_t>(
        std::abs(static_cast<std::int32_t>(center) -
                 static_cast<std::int32_t>(neighbor)));
    result = std::max(result, difference);
  }
  return result;
}

} // namespace aetheria::site::detail

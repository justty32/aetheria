#include "core/site/site_skeleton_detail.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>

namespace aetheria::site::detail {
namespace {

[[nodiscard]] std::uint32_t manhattan(SiteXY left, SiteXY right) noexcept {
  const auto dx = std::abs(static_cast<std::int32_t>(left.x) - right.x);
  const auto dy = std::abs(static_cast<std::int32_t>(left.y) - right.y);
  return static_cast<std::uint32_t>(dx + dy);
}

[[nodiscard]] std::vector<std::uint16_t>
water_distances(const SiteSkeleton &skeleton) {
  constexpr auto kFar = std::numeric_limits<std::uint16_t>::max() / 2U;
  std::vector<std::uint16_t> distance(kSiteTileCount, kFar);
  for (std::size_t index = 0; index < kSiteTileCount; ++index) {
    if (skeleton.water[index] != 0) {
      distance[index] = 0;
    }
  }
  for (std::uint16_t y = 0; y < kSiteHeight; ++y) {
    for (std::uint16_t x = 0; x < kSiteWidth; ++x) {
      auto &value = distance[tile_index(x, y)];
      if (x > 0) {
        value = std::min<std::uint16_t>(
            value,
            static_cast<std::uint16_t>(distance[tile_index(x - 1U, y)] + 1U));
      }
      if (y > 0) {
        value = std::min<std::uint16_t>(
            value,
            static_cast<std::uint16_t>(distance[tile_index(x, y - 1U)] + 1U));
      }
    }
  }
  for (std::uint16_t y = kSiteHeight; y-- > 0;) {
    for (std::uint16_t x = kSiteWidth; x-- > 0;) {
      auto &value = distance[tile_index(x, y)];
      if (x + 1U < kSiteWidth) {
        value = std::min<std::uint16_t>(
            value,
            static_cast<std::uint16_t>(distance[tile_index(x + 1U, y)] + 1U));
      }
      if (y + 1U < kSiteHeight) {
        value = std::min<std::uint16_t>(
            value,
            static_cast<std::uint16_t>(distance[tile_index(x, y + 1U)] + 1U));
      }
    }
  }
  return distance;
}

} // namespace

SiteXY choose_city_center(const SiteSkeleton &skeleton) {
  const auto water_distance = water_distances(skeleton);
  SiteXY best{kSiteWidth / 2U, kSiteHeight / 2U};
  auto best_score = std::numeric_limits<std::uint64_t>::max();
  for (std::uint16_t y = 4; y < kSiteHeight - 4U; ++y) {
    for (std::uint16_t x = 4; x < kSiteWidth - 4U; ++x) {
      const auto index = tile_index(x, y);
      if (skeleton.water[index] != 0) {
        continue;
      }
      const SiteXY candidate{x, y};
      std::uint64_t gate_distance{};
      for (const auto &gate : skeleton.gates) {
        gate_distance += manhattan(candidate, gate.tile);
      }
      const auto water =
          water_distance[index] >= 1024U
              ? 0U
              : static_cast<std::uint32_t>(std::abs(
                    static_cast<std::int32_t>(water_distance[index]) - 8));
      const auto center_distance =
          manhattan(candidate, {kSiteWidth / 2U, kSiteHeight / 2U});
      const auto score =
          static_cast<std::uint64_t>(local_slope(skeleton, x, y)) * 64U +
          static_cast<std::uint64_t>(water) * 8U + gate_distance * 2U +
          center_distance;
      if (score < best_score) {
        best_score = score;
        best = candidate;
      }
    }
  }
  return best;
}

} // namespace aetheria::site::detail

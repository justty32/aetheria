#include "core/site/site_skeleton_detail.h"

#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <cstdint>

namespace aetheria::site::detail {
namespace {

constexpr std::uint64_t kBlockSalt = UINT64_C(0x4F12D6A87B39C5E1);

struct Rect {
  std::uint16_t x{};
  std::uint16_t y{};
  std::uint16_t width{};
  std::uint16_t height{};
};

void split_block(SiteSkeleton &skeleton, Rect rect, std::uint8_t depth,
                 std::uint64_t seed, const rules::SiteGenerationRules &config) {
  const auto split_vertical =
      rect.width > rect.height ||
      (rect.width == rect.height && (worldgen::splitmix64(seed) & 1U) != 0);
  const auto extent = split_vertical ? rect.width : rect.height;
  if (depth >= config.block_split_depth ||
      extent < static_cast<std::uint16_t>(config.block_min_extent * 2U + 1U)) {
    skeleton.blocks.push_back({{rect.x, rect.y}, rect.width, rect.height});
    return;
  }

  const auto random = worldgen::splitmix64(seed ^ depth);
  const auto percent_span = static_cast<std::uint16_t>(
      config.block_cut_max_percent - config.block_cut_min_percent + 1U);
  auto percent = static_cast<std::uint16_t>(config.block_cut_min_percent +
                                            random % percent_span);
  if ((random & UINT64_C(0x100)) != 0) {
    percent = static_cast<std::uint16_t>(100U - percent);
  }
  auto first_extent = static_cast<std::uint16_t>(extent * percent / 100U);
  first_extent = std::clamp<std::uint16_t>(
      first_extent, config.block_min_extent,
      static_cast<std::uint16_t>(extent - config.block_min_extent - 1U));
  const auto second_extent =
      static_cast<std::uint16_t>(extent - first_extent - 1U);

  if (split_vertical) {
    const auto street_x = static_cast<std::uint16_t>(rect.x + first_extent);
    for (std::uint16_t y = rect.y; y < rect.y + rect.height; ++y) {
      skeleton.roads[tile_index(street_x, y)] = UINT8_C(1);
    }
    split_block(skeleton, {rect.x, rect.y, first_extent, rect.height},
                depth + 1U, worldgen::splitmix64(seed ^ UINT64_C(0xA1)),
                config);
    split_block(skeleton,
                {static_cast<std::uint16_t>(street_x + 1U), rect.y,
                 second_extent, rect.height},
                depth + 1U, worldgen::splitmix64(seed ^ UINT64_C(0xB2)),
                config);
  } else {
    const auto street_y = static_cast<std::uint16_t>(rect.y + first_extent);
    for (std::uint16_t x = rect.x; x < rect.x + rect.width; ++x) {
      skeleton.roads[tile_index(x, street_y)] = UINT8_C(1);
    }
    split_block(skeleton, {rect.x, rect.y, rect.width, first_extent},
                depth + 1U, worldgen::splitmix64(seed ^ UINT64_C(0xC3)),
                config);
    split_block(skeleton,
                {rect.x, static_cast<std::uint16_t>(street_y + 1U), rect.width,
                 second_extent},
                depth + 1U, worldgen::splitmix64(seed ^ UINT64_C(0xD4)),
                config);
  }
}

} // namespace

void generate_site_blocks(SiteSkeleton &skeleton, std::uint64_t site_seed,
                          const rules::Ruleset &ruleset) {
  const auto &config = ruleset.site_generation_rules();
  split_block(skeleton, {1, 1, kSiteWidth - 2U, kSiteHeight - 2U}, 0,
              site_seed ^ kBlockSalt, config);
}

void mark_site_buildable(SiteSkeleton &skeleton,
                         const rules::Ruleset &ruleset) {
  const auto max_slope = ruleset.site_generation_rules().max_buildable_slope;
  for (std::uint16_t y = 0; y < kSiteHeight; ++y) {
    for (std::uint16_t x = 0; x < kSiteWidth; ++x) {
      const auto index = tile_index(x, y);
      skeleton.buildable[index] =
          skeleton.water[index] == 0 && skeleton.roads[index] == 0 &&
                  local_slope(skeleton, x, y) <= max_slope
              ? UINT8_C(1)
              : UINT8_C(0);
    }
  }
}

} // namespace aetheria::site::detail

#include <cstdint>

#include "core/site/site_skeleton_detail.h"
#include "core/spatial/recursive_partition.h"

namespace aetheria::site::detail {
namespace {

constexpr std::uint64_t kBlockSalt = UINT64_C(0x4F12D6A87B39C5E1);

} // namespace

void generate_site_blocks(SiteSkeleton &skeleton, std::uint64_t site_seed,
                          const rules::Ruleset &ruleset) {
  const auto &config = ruleset.site_generation_rules();
  const auto partition = spatial::partition_rect(
      {1, 1, kSiteWidth - 2U, kSiteHeight - 2U}, site_seed ^ kBlockSalt,
      {config.block_split_depth, config.block_cut_min_percent,
       config.block_cut_max_percent, config.block_min_extent, 1});
  for (const auto &leaf : partition.leaves) {
    skeleton.blocks.push_back({{leaf.x, leaf.y}, leaf.width, leaf.height});
  }
  for (const auto &cut : partition.cuts) {
    for (std::uint16_t offset = 0; offset < cut.extent; ++offset) {
      const auto x = cut.vertical
                         ? cut.coordinate
                         : static_cast<std::uint16_t>(cut.start + offset);
      const auto y = cut.vertical
                         ? static_cast<std::uint16_t>(cut.start + offset)
                         : cut.coordinate;
      skeleton.roads[tile_index(x, y)] = UINT8_C(1);
    }
  }
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

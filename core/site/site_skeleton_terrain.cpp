#include "core/site/site_skeleton_detail.h"

#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace aetheria::site::detail {
namespace {

constexpr std::uint64_t kHeightSalt = UINT64_C(0xC142A0D78B65E39F);

[[nodiscard]] std::int32_t signed_sample(std::uint64_t seed,
                                         std::uint32_t amplitude) noexcept {
  const auto span = static_cast<std::uint64_t>(amplitude) * 2U + 1U;
  return static_cast<std::int32_t>(worldgen::splitmix64(seed) % span) -
         static_cast<std::int32_t>(amplitude);
}

[[nodiscard]] std::int32_t lerp(std::int32_t from, std::int32_t to,
                                std::uint32_t numerator,
                                std::uint32_t denominator) noexcept {
  return from +
         static_cast<std::int32_t>(
             (static_cast<std::int64_t>(to - from) * numerator) / denominator);
}

[[nodiscard]] SiteXY boundary_tile(SiteBoundarySide side,
                                   std::uint16_t position) noexcept {
  switch (side) {
  case SiteBoundarySide::North:
    return {position, 0};
  case SiteBoundarySide::East:
    return {kSiteWidth - 1U, position};
  case SiteBoundarySide::South:
    return {position, kSiteHeight - 1U};
  case SiteBoundarySide::West:
    return {0, position};
  }
  return {};
}

void mark_boundary_edge(SiteSkeleton &skeleton, SiteBoundarySide side,
                        std::uint16_t position, rules::EdgeId edge) {
  const auto tile = boundary_tile(side, position);
  skeleton.edges[tile_index(tile) * kDirections + side_index(side)] = edge;
}

void mark_water(SiteSkeleton &skeleton, std::int32_t x, std::int32_t y,
                rules::GroundId water_ground) {
  if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(kSiteWidth) ||
      y >= static_cast<std::int32_t>(kSiteHeight)) {
    return;
  }
  const auto index =
      tile_index(static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y));
  skeleton.water[index] = UINT8_C(1);
  skeleton.ground[index] = water_ground;
}

void carve_boundary_water(SiteSkeleton &skeleton, SiteBoundarySide side,
                          std::uint16_t position, std::uint8_t reach,
                          std::uint8_t half_width, std::uint64_t seed,
                          rules::GroundId water_ground) {
  for (std::uint16_t depth = 0; depth <= reach; ++depth) {
    const auto bend = depth == 0 ? 0 : signed_sample(seed ^ (depth / 4U), 2);
    for (std::int32_t width = -half_width; width <= half_width; ++width) {
      const auto lateral = static_cast<std::int32_t>(position) + bend + width;
      switch (side) {
      case SiteBoundarySide::North:
        mark_water(skeleton, lateral, depth, water_ground);
        break;
      case SiteBoundarySide::East:
        mark_water(skeleton, static_cast<std::int32_t>(kSiteWidth - 1U - depth),
                   lateral, water_ground);
        break;
      case SiteBoundarySide::South:
        mark_water(skeleton, lateral,
                   static_cast<std::int32_t>(kSiteHeight - 1U - depth),
                   water_ground);
        break;
      case SiteBoundarySide::West:
        mark_water(skeleton, depth, lateral, water_ground);
        break;
      }
    }
  }
}

} // namespace

void generate_site_terrain(SiteSkeleton &skeleton, const SiteSlowVars &slow,
                           std::uint64_t site_seed,
                           const rules::Ruleset &ruleset) {
  const auto *mapping = ruleset.terrain_ground_mapping(slow.base);
  const auto water_ground = ruleset.find_ground("ground.water");
  if (mapping == nullptr || !water_ground.has_value()) {
    throw std::runtime_error{"Site 地形生成缺少 ground 映射或 ground.water"};
  }
  const auto &config = ruleset.site_generation_rules();
  const auto relief_scale = static_cast<std::uint32_t>(
      std::max(1, ruleset.relief(slow.relief)->move_cost));
  const auto amplitude =
      static_cast<std::uint32_t>(config.height_noise_amplitude) * relief_scale;
  const auto terrain_seed = worldgen::splitmix64(
      site_seed ^
      (static_cast<std::uint64_t>(rules::value_of(slow.feature)) << 32U));

  std::array<std::int32_t, 4> corners{};
  for (std::size_t index = 0; index < corners.size(); ++index) {
    corners[index] =
        signed_sample(terrain_seed ^ kHeightSalt ^ index, amplitude / 2U);
  }
  std::array<std::int32_t, 25> noise{};
  for (std::size_t index = 0; index < noise.size(); ++index) {
    noise[index] = signed_sample(
        terrain_seed ^ kHeightSalt ^ UINT64_C(0x100) ^ index, amplitude / 3U);
  }

  for (std::uint16_t y = 0; y < kSiteHeight; ++y) {
    for (std::uint16_t x = 0; x < kSiteWidth; ++x) {
      const auto north = lerp(corners[0], corners[1], x, kSiteWidth - 1U);
      const auto south = lerp(corners[3], corners[2], x, kSiteWidth - 1U);
      const auto base_offset = lerp(north, south, y, kSiteHeight - 1U);
      const auto scaled_x = static_cast<std::uint32_t>(x) * 4U;
      const auto scaled_y = static_cast<std::uint32_t>(y) * 4U;
      const auto cell_x = std::min<std::uint32_t>(scaled_x / 63U, 3U);
      const auto cell_y = std::min<std::uint32_t>(scaled_y / 63U, 3U);
      const auto fraction_x = scaled_x - cell_x * 63U;
      const auto fraction_y = scaled_y - cell_y * 63U;
      const auto top = lerp(noise[cell_y * 5U + cell_x],
                            noise[cell_y * 5U + cell_x + 1U], fraction_x, 63U);
      const auto bottom =
          lerp(noise[(cell_y + 1U) * 5U + cell_x],
               noise[(cell_y + 1U) * 5U + cell_x + 1U], fraction_x, 63U);
      const auto value = static_cast<std::int32_t>(slow.elevation) +
                         base_offset + lerp(top, bottom, fraction_y, 63U);
      skeleton.elevation[tile_index(x, y)] = static_cast<std::uint16_t>(
          std::clamp(value, 0, static_cast<std::int32_t>(UINT16_MAX)));
    }
  }

  const auto *terrain = ruleset.terrain(slow.base);
  if ((terrain->flags & rules::kTerrainWaterFlag) != 0) {
    std::ranges::fill(skeleton.water, UINT8_C(1));
    std::ranges::fill(skeleton.ground, *water_ground);
  }
  for (std::size_t index = 0; index < slow.edges.size(); ++index) {
    const auto side = static_cast<SiteBoundarySide>(index);
    const auto position = boundary_position(site_seed, side);
    const auto *edge = ruleset.edge(slow.edges[index]);
    if (edge->flags != 0) {
      mark_boundary_edge(skeleton, side, position, slow.edges[index]);
    }
    if ((edge->flags & rules::kEdgeRiverFlag) != 0) {
      const auto half_width =
          static_cast<std::uint8_t>(std::clamp(edge->move_cost, 1, 3));
      carve_boundary_water(skeleton, side, position, config.water_inland_reach,
                           half_width, site_seed ^ index, *water_ground);
    }
  }
}

} // namespace aetheria::site::detail

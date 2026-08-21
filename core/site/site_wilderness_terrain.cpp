#include "core/site/site_wilderness_detail.h"

#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace aetheria::site::wilderness_detail {
namespace {

constexpr std::uint64_t kTerrainSalt = UINT64_C(0x5E2A91C47B38D60F);

[[nodiscard]] std::int32_t signed_noise(std::uint64_t seed, std::uint32_t amplitude) noexcept {
    const auto span = static_cast<std::uint64_t>(amplitude) * 2U + 1U;
    return static_cast<std::int32_t>(worldgen::splitmix64(seed) % span) -
           static_cast<std::int32_t>(amplitude);
}

void apply_boundary(WildernessSkeleton& result, SiteBoundarySide side,
                    const BoundaryProfile& profile) {
    for (std::uint8_t position = 0; position < kSiteWidth; ++position) {
        const auto tile = boundary_tile(side, position);
        const auto index = tile_index(tile);
        result.terrain.elevation[index] = profile.elevation[position];
        result.terrain.ground[index] = profile.ground[position];
        result.terrain.water[index] = profile.water_depth[position];
        result.terrain.edges[index * kDirections + static_cast<std::size_t>(side)] =
            profile.edges[position];
    }
}

[[nodiscard]] std::uint16_t local_slope(const SiteSkeleton& terrain, SiteXY tile) noexcept {
    const auto center = terrain.elevation[tile_index(tile)];
    std::uint16_t maximum{};
    constexpr std::array<std::int16_t, 4> dx{0, 1, 0, -1};
    constexpr std::array<std::int16_t, 4> dy{-1, 0, 1, 0};
    for (std::size_t direction = 0; direction < dx.size(); ++direction) {
        const auto x = static_cast<std::int32_t>(tile.x) + dx[direction];
        const auto y = static_cast<std::int32_t>(tile.y) + dy[direction];
        if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(kSiteWidth) ||
            y >= static_cast<std::int32_t>(kSiteHeight)) {
            continue;
        }
        const auto adjacent = terrain.elevation[tile_index(
            {static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y)})];
        maximum = std::max(maximum, static_cast<std::uint16_t>(
                                           std::abs(static_cast<std::int32_t>(center) - adjacent)));
    }
    return maximum;
}

}  // namespace

SiteXY boundary_tile(SiteBoundarySide side, std::uint8_t position) noexcept {
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

void generate_wilderness_terrain(WildernessSkeleton& result, const WildernessSlowVars& slow,
                                 std::uint64_t site_seed, const rules::Ruleset& ruleset) {
    const auto& config = ruleset.wilderness_generation_rules();
    const auto* mapping = ruleset.terrain_ground_mapping(slow.local.base);
    const auto* terrain_definition = ruleset.terrain(slow.local.base);
    const auto* relief_definition = ruleset.relief(slow.local.relief);
    const auto no_edge = ruleset.find_edge("edge.none");
    if (!config.loaded || mapping == nullptr || terrain_definition == nullptr ||
        relief_definition == nullptr || !no_edge.has_value()) {
        throw std::runtime_error{"荒野 W1 缺少資料規則或 def"};
    }

    auto& terrain = result.terrain;
    terrain.ground.assign(kSiteTileCount, mapping->ground);
    terrain.edges.assign(kSiteTileCount * kDirections, *no_edge);
    terrain.elevation.resize(kSiteTileCount);
    terrain.water.assign(kSiteTileCount, 0);
    terrain.roads.assign(kSiteTileCount, 0);
    terrain.buildable.assign(kSiteTileCount, 0);
    terrain.city_center = {kSiteWidth / 2U, kSiteHeight / 2U};
    const auto relief_scale = static_cast<std::uint32_t>(
        std::clamp(relief_definition->move_cost, 1, 6));
    const auto amplitude = static_cast<std::uint32_t>(config.height_noise_amplitude) * relief_scale;
    const bool sea = (terrain_definition->flags & rules::kTerrainWaterFlag) != 0;

    const auto& north = slow.boundaries[static_cast<std::size_t>(SiteBoundarySide::North)];
    const auto& east = slow.boundaries[static_cast<std::size_t>(SiteBoundarySide::East)];
    const auto& south = slow.boundaries[static_cast<std::size_t>(SiteBoundarySide::South)];
    const auto& west = slow.boundaries[static_cast<std::size_t>(SiteBoundarySide::West)];
    for (std::uint16_t y = 0; y < kSiteHeight; ++y) {
        for (std::uint16_t x = 0; x < kSiteWidth; ++x) {
            const auto index = tile_index({x, y});
            const auto vertical = static_cast<std::int64_t>(north.elevation[x]) +
                (static_cast<std::int64_t>(south.elevation[x]) - north.elevation[x]) * y / 63;
            const auto horizontal = static_cast<std::int64_t>(west.elevation[y]) +
                (static_cast<std::int64_t>(east.elevation[y]) - west.elevation[y]) * x / 63;
            const auto edge_distance = std::min({x, y, static_cast<std::uint16_t>(63U - x),
                                                 static_cast<std::uint16_t>(63U - y)});
            const auto noise = signed_noise(site_seed ^ kTerrainSalt ^ index, amplitude);
            const auto elevation = (vertical + horizontal) / 2 +
                                   static_cast<std::int64_t>(noise) * edge_distance / 31;
            terrain.elevation[index] = static_cast<std::uint16_t>(
                std::clamp<std::int64_t>(elevation, 0, UINT16_MAX));
            if (!sea && (worldgen::splitmix64(site_seed ^ UINT64_C(0xA100) ^ index) % 100U) <
                            18U * relief_scale) {
                terrain.ground[index] = mapping->rough_ground;
            }
            if (sea) {
                terrain.water[index] = static_cast<std::uint8_t>(
                    1U + std::min<std::uint16_t>(7U, edge_distance / 8U));
            }
        }
    }

    result.boundaries = slow.boundaries;
    for (std::size_t side = 0; side < slow.boundaries.size(); ++side) {
        apply_boundary(result, static_cast<SiteBoundarySide>(side), slow.boundaries[side]);
    }

    const auto slope_limit = relief_definition->move_cost <= 1
                                 ? config.plain_passable_slope
                                 : (relief_definition->move_cost <= 2
                                        ? config.hills_passable_slope
                                        : config.mountain_passable_slope);
    for (std::uint16_t y = 0; y < kSiteHeight; ++y) {
        for (std::uint16_t x = 0; x < kSiteWidth; ++x) {
            const auto index = tile_index({x, y});
            terrain.buildable[index] = static_cast<std::uint8_t>(
                terrain.water[index] == 0 && local_slope(terrain, {x, y}) <= slope_limit);
        }
    }
}

}  // namespace aetheria::site::wilderness_detail

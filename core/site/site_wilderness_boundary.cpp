#include "core/site/site_wilderness.h"

#include "core/site/site_wilderness_boundary_detail.h"
#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace aetheria::site {
namespace {

constexpr std::uint64_t kEdgeSalt = UINT64_C(0x4A8E9137D52CB60F);
constexpr std::uint64_t kCornerSalt = UINT64_C(0x8D1C6B42F370A95E);
using namespace wilderness_boundary_detail;

[[nodiscard]] BoundaryProfile build_profile(const world::RegionTiles& tiles,
                                             world::RegionXY coordinate,
                                             SiteBoundarySide side, std::uint64_t world_seed,
                                             std::uint32_t region_id,
                                             const rules::Ruleset& ruleset) {
    const auto local_index = tiles.index_of(coordinate);
    const auto neighbor = neighbor_of(tiles, coordinate, side);
    const auto edge_id = neighbor.has_value()
                             ? internal_edge_id(local_index, tiles.index_of(*neighbor), tiles.width)
                             : static_cast<std::uint64_t>(tiles.tile_count()) * 2U +
                                   local_index * 4U + static_cast<std::size_t>(side);
    const auto edge_seed = worldgen::splitmix64(world_seed ^ region_id ^ kEdgeSalt ^ edge_id);
    const auto corners = edge_corners(coordinate, side);
    const auto first_seed = worldgen::splitmix64(
        world_seed ^ region_id ^ kCornerSalt ^ corner_id(corners[0], corners[1], tiles.width));
    const auto second_seed = worldgen::splitmix64(
        world_seed ^ region_id ^ kCornerSalt ^ corner_id(corners[2], corners[3], tiles.width));
    const auto first = sample_corner(tiles, corners[0], corners[1], first_seed, ruleset);
    const auto second = sample_corner(tiles, corners[2], corners[3], second_seed, ruleset);
    const auto no_edge = ruleset.find_edge("edge.none");
    if (!no_edge.has_value()) {
        throw std::runtime_error{"荒野邊界缺少 edge.none"};
    }

    const auto other_index = neighbor.has_value() ? tiles.index_of(*neighbor) : local_index;
    const auto low_index = std::min(local_index, other_index);
    const auto high_index = std::max(local_index, other_index);
    BoundaryProfile result;
    result.edges.fill(*no_edge);
    for (std::size_t position = 0; position < kSiteWidth; ++position) {
        const auto numerator = static_cast<std::int64_t>(position);
        const auto interpolated = static_cast<std::int64_t>(first.elevation) +
            (static_cast<std::int64_t>(second.elevation) - first.elevation) * numerator / 63;
        const auto fade = static_cast<std::int64_t>(std::min(position, kSiteWidth - 1U - position));
        const auto noise = static_cast<std::int64_t>(worldgen::splitmix64(edge_seed ^ position) % 17U) - 8;
        result.elevation[position] = static_cast<std::uint16_t>(std::clamp<std::int64_t>(
            interpolated + noise * fade / 31, 0, UINT16_MAX));
        const auto selected =
            (worldgen::splitmix64(edge_seed ^ UINT64_C(0x6000) ^ position) & 1U)
                ? low_index
                : high_index;
        const auto* mapping = ruleset.terrain_ground_mapping(tiles.base[selected]);
        if (mapping == nullptr) {
            throw std::runtime_error{"荒野邊界缺少 Terrain→Ground 映射"};
        }
        result.ground[position] = mapping->ground;
        const auto* ground = ruleset.ground(mapping->ground);
        result.water_depth[position] = static_cast<std::uint8_t>(
            ground != nullptr && (ground->flags & rules::kGroundWaterFlag) != 0);
    }
    result.elevation.front() = first.elevation;
    result.elevation.back() = second.elevation;
    result.ground.front() = first.ground;
    result.ground.back() = second.ground;
    result.water_depth.front() = first.water_depth;
    result.water_depth.back() = second.water_depth;

    const auto edge = tiles.edges[local_index * 4U + static_cast<std::size_t>(side)];
    const auto* definition = ruleset.edge(edge);
    if (definition == nullptr) {
        throw std::runtime_error{"荒野邊界引用不存在的 EdgeDef"};
    }
    constexpr auto crossing_flags = rules::kEdgeRoadFlag | rules::kEdgeRiverFlag;
    if ((definition->flags & crossing_flags) != 0) {
        const auto position = static_cast<std::uint8_t>(4U + edge_seed % 56U);
        const auto river_width = static_cast<std::uint8_t>(
            (definition->flags & rules::kEdgeRiverFlag) != 0
                ? std::clamp(definition->move_cost, 1, 4)
                : 1);
        const auto width = static_cast<std::uint8_t>(
            (definition->flags & rules::kEdgeRoadFlag) != 0
                ? std::max<std::uint8_t>(UINT8_C(2), river_width)
                                                            : river_width);
        result.crossings.push_back({position, width, edge});
        const auto water_ground = ruleset.find_ground("ground.water");
        const auto half = static_cast<std::int32_t>(width / 2U);
        for (std::int32_t offset = -half; offset <= half; ++offset) {
            const auto sample = static_cast<std::int32_t>(position) + offset;
            if (sample < 0 || sample >= static_cast<std::int32_t>(kSiteWidth)) {
                continue;
            }
            result.edges[static_cast<std::size_t>(sample)] = edge;
            if ((definition->flags & rules::kEdgeRiverFlag) != 0 && water_ground.has_value()) {
                result.ground[static_cast<std::size_t>(sample)] = *water_ground;
                result.water_depth[static_cast<std::size_t>(sample)] = river_width;
            }
        }
    }
    return result;
}

}  // namespace

WildernessSlowVars project_wilderness_slow_vars(const world::RegionTiles& tiles,
                                                 world::RegionXY coordinate,
                                                 std::uint64_t world_seed,
                                                 std::uint32_t region_id,
                                                 const rules::Ruleset& ruleset) {
    if (!tiles.valid_layout()) {
        throw std::runtime_error{"無法從版面無效的 RegionTiles 投影荒野邊界"};
    }
    WildernessSlowVars result;
    result.local = split_site_vars(tiles, coordinate).slow;
    for (std::size_t side = 0; side < result.boundaries.size(); ++side) {
        result.boundaries[side] = build_profile(tiles, coordinate,
                                               static_cast<SiteBoundarySide>(side), world_seed,
                                               region_id, ruleset);
    }
    return result;
}

}  // namespace aetheria::site

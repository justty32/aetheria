#pragma once

// boundary_profile.h：L1→L2 與 L2→L3 共用的規範邊界識別、角錨定與一維剖面生成。

#include "core/rules/ruleset.h"
#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace aetheria::spatial {

inline constexpr std::uint32_t kBoundarySampleCount = 64;
inline constexpr std::uint64_t kBoundaryEdgeSalt = UINT64_C(0x4A8E9137D52CB60F);
inline constexpr std::uint64_t kBoundaryCornerSalt = UINT64_C(0x8D1C6B42F370A95E);

enum class BoundarySide : std::uint8_t {
    North,
    East,
    South,
    West,
};

struct BoundaryCrossing {
    std::uint8_t pos{};
    std::uint8_t width{};
    rules::EdgeId kind{};

    constexpr bool operator==(const BoundaryCrossing&) const noexcept = default;
};

struct BoundaryProfile {
    std::array<std::uint16_t, kBoundarySampleCount> elevation{};
    std::array<rules::GroundId, kBoundarySampleCount> ground{};
    std::array<std::uint8_t, kBoundarySampleCount> water_depth{};
    std::array<rules::EdgeId, kBoundarySampleCount> edges{};
    std::vector<BoundaryCrossing> crossings;

    bool operator==(const BoundaryProfile&) const = default;
};

[[nodiscard]] constexpr std::uint64_t canonical_edge_id(std::size_t first,
                                                        std::size_t second,
                                                        std::uint32_t width) noexcept {
    const auto low = std::min(first, second);
    const bool south = std::max(first, second) - low == width;
    return static_cast<std::uint64_t>(low) * 2U + static_cast<std::uint64_t>(south);
}

[[nodiscard]] constexpr std::uint64_t canonical_corner_id(std::uint32_t x,
                                                          std::uint32_t y,
                                                          std::uint32_t width) noexcept {
    return static_cast<std::uint64_t>(y) * (width + 1U) + x;
}

namespace detail {

struct CornerSample {
    std::uint16_t elevation{};
    rules::GroundId ground{};
    std::uint8_t water_depth{};
};

[[nodiscard]] constexpr std::array<std::uint32_t, 4> edge_corners(
    std::uint32_t x, std::uint32_t y, BoundarySide side) noexcept {
    switch (side) {
    case BoundarySide::North:
        return {x, y, x + 1U, y};
    case BoundarySide::East:
        return {x + 1U, y, x + 1U, y + 1U};
    case BoundarySide::South:
        return {x, y + 1U, x + 1U, y + 1U};
    case BoundarySide::West:
        return {x, y, x, y + 1U};
    }
    return {};
}

template <typename Source>
[[nodiscard]] CornerSample sample_corner(const Source& source, std::uint32_t corner_x,
                                         std::uint32_t corner_y, std::uint64_t seed) {
    std::array<std::size_t, 4> candidates{};
    std::size_t count{};
    for (std::int32_t dy = -1; dy <= 0; ++dy) {
        for (std::int32_t dx = -1; dx <= 0; ++dx) {
            const auto x = static_cast<std::int32_t>(corner_x) + dx;
            const auto y = static_cast<std::int32_t>(corner_y) + dy;
            if (x >= 0 && y >= 0 && x < static_cast<std::int32_t>(source.width()) &&
                y < static_cast<std::int32_t>(source.height())) {
                candidates[count++] = source.index(static_cast<std::uint32_t>(x),
                                                   static_cast<std::uint32_t>(y));
            }
        }
    }
    if (count == 0) {
        throw std::logic_error{"邊界角點沒有相鄰父層 tile"};
    }
    std::uint64_t elevation_sum{};
    for (std::size_t index = 0; index < count; ++index) {
        elevation_sum += source.elevation(candidates[index]);
    }
    const auto selected = candidates[static_cast<std::size_t>(seed % count)];
    return {static_cast<std::uint16_t>(elevation_sum / count), source.ground(selected),
            source.water_depth(selected)};
}

}  // namespace detail

// Source 提供 width/height/index/elevation/ground/water_depth/edge；演算法只讀父層慢變數。
template <typename Source>
[[nodiscard]] BoundaryProfile build_boundary_profile(const Source& source, std::uint32_t x,
                                                     std::uint32_t y, BoundarySide side,
                                                     std::uint64_t parent_seed,
                                                     const rules::Ruleset& ruleset) {
    if (x >= source.width() || y >= source.height()) {
        throw std::out_of_range{"邊界剖面的父層座標超界"};
    }
    const auto local_index = source.index(x, y);
    auto neighbor_x = static_cast<std::int32_t>(x);
    auto neighbor_y = static_cast<std::int32_t>(y);
    switch (side) {
    case BoundarySide::North:
        --neighbor_y;
        break;
    case BoundarySide::East:
        ++neighbor_x;
        break;
    case BoundarySide::South:
        ++neighbor_y;
        break;
    case BoundarySide::West:
        --neighbor_x;
        break;
    }
    const bool has_neighbor = neighbor_x >= 0 && neighbor_y >= 0 &&
                              neighbor_x < static_cast<std::int32_t>(source.width()) &&
                              neighbor_y < static_cast<std::int32_t>(source.height());
    const auto other_index = has_neighbor
                                 ? source.index(static_cast<std::uint32_t>(neighbor_x),
                                                static_cast<std::uint32_t>(neighbor_y))
                                 : local_index;
    const auto edge_id = has_neighbor
                             ? canonical_edge_id(local_index, other_index, source.width())
                             : source.tile_count() * 2U + local_index * 4U +
                                   static_cast<std::size_t>(side);
    const auto edge_seed =
        worldgen::splitmix64(parent_seed ^ kBoundaryEdgeSalt ^ edge_id);
    const auto corners = detail::edge_corners(x, y, side);
    const auto first_seed = worldgen::splitmix64(
        parent_seed ^ kBoundaryCornerSalt ^
        canonical_corner_id(corners[0], corners[1], source.width()));
    const auto second_seed = worldgen::splitmix64(
        parent_seed ^ kBoundaryCornerSalt ^
        canonical_corner_id(corners[2], corners[3], source.width()));
    const auto first = detail::sample_corner(source, corners[0], corners[1], first_seed);
    const auto second = detail::sample_corner(source, corners[2], corners[3], second_seed);
    const auto no_edge = ruleset.find_edge("edge.none");
    if (!no_edge.has_value()) {
        throw std::runtime_error{"邊界剖面缺少 edge.none"};
    }

    BoundaryProfile result;
    result.edges.fill(*no_edge);
    const auto low_index = std::min(local_index, other_index);
    const auto high_index = std::max(local_index, other_index);
    for (std::size_t position = 0; position < kBoundarySampleCount; ++position) {
        const auto interpolated = static_cast<std::int64_t>(first.elevation) +
            (static_cast<std::int64_t>(second.elevation) - first.elevation) *
                static_cast<std::int64_t>(position) /
                static_cast<std::int64_t>(kBoundarySampleCount - 1U);
        const auto fade = static_cast<std::int64_t>(
            std::min(position, static_cast<std::size_t>(kBoundarySampleCount - 1U - position)));
        const auto noise = static_cast<std::int64_t>(
                               worldgen::splitmix64(edge_seed ^ position) % 17U) -
                           8;
        result.elevation[position] = static_cast<std::uint16_t>(std::clamp<std::int64_t>(
            interpolated + noise * fade / 31, 0, UINT16_MAX));
        const auto selected =
            (worldgen::splitmix64(edge_seed ^ UINT64_C(0x6000) ^ position) & 1U) != 0
                ? low_index
                : high_index;
        result.ground[position] = source.ground(selected);
        result.water_depth[position] = source.water_depth(selected);
    }
    result.elevation.front() = first.elevation;
    result.elevation.back() = second.elevation;
    result.ground.front() = first.ground;
    result.ground.back() = second.ground;
    result.water_depth.front() = first.water_depth;
    result.water_depth.back() = second.water_depth;

    const auto edge = source.edge(local_index, side);
    const auto* definition = ruleset.edge(edge);
    if (definition == nullptr) {
        throw std::runtime_error{"邊界剖面引用不存在的 EdgeDef"};
    }
    if ((definition->flags & rules::kEdgeWallFlag) != 0) {
        result.edges.fill(edge);
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
            if (sample < 0 || sample >= static_cast<std::int32_t>(kBoundarySampleCount)) {
                continue;
            }
            const auto sample_index = static_cast<std::size_t>(sample);
            result.edges[sample_index] = edge;
            if ((definition->flags & rules::kEdgeRiverFlag) != 0 && water_ground.has_value()) {
                result.ground[sample_index] = *water_ground;
                result.water_depth[sample_index] = river_width;
            }
        }
    }
    return result;
}

}  // namespace aetheria::spatial

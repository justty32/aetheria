#pragma once

// local_generation_detail.h：開放 Local 各生成階段共用的座標與內部入口。

#include "core/local/local_tiles.h"

namespace aetheria::local::detail::generation {

inline constexpr std::size_t kDirections = 4;

inline constexpr std::array<std::uint32_t, 2> kPathFlags{rules::kEdgeRoadFlag,
                                                  rules::kEdgeRiverFlag};

struct PathAnchor {
    LocalXY tile;
    spatial::BoundarySide side{};
    std::uint8_t width{};
    std::uint32_t flags{};
};

[[nodiscard]] constexpr std::size_t tile_index(std::uint16_t x, std::uint16_t y) noexcept {
    return static_cast<std::size_t>(y) * kLocalWidth + x;
}

[[nodiscard]] constexpr LocalXY boundary_tile(spatial::BoundarySide side,
                                              std::uint8_t position) noexcept {
    switch (side) {
    case spatial::BoundarySide::North:
        return {position, 0};
    case spatial::BoundarySide::East:
        return {kLocalWidth - 1U, position};
    case spatial::BoundarySide::South:
        return {position, kLocalHeight - 1U};
    case spatial::BoundarySide::West:
        return {0, position};
    }
    return {};
}

[[nodiscard]] std::vector<PathAnchor> collect_path_anchors(
    const OpenLocalSkeleton& skeleton, const rules::Ruleset& ruleset);
void connect_crossings(OpenLocalSkeleton& result, rules::GroundId water_ground,
                       const rules::Ruleset& ruleset);
void texture_ground(OpenLocalSkeleton& result, std::uint64_t local_seed,
                    const rules::Ruleset& ruleset);
void scatter(OpenLocalSkeleton& result, const LocalSlowVars& slow, std::uint64_t local_seed,
             const rules::Ruleset& ruleset);
void place_objects(OpenLocalSkeleton& result, const LocalSlowVars& slow,
                   std::uint64_t local_seed, const rules::Ruleset& ruleset);

}  // namespace aetheria::local::detail::generation

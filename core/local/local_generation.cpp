// local_generation.cpp：開放 Local 骨架生成協調、地形與邊界套用。

#include "core/local/local_tiles.h"

#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>

#include "core/local/local_generation_detail.h"

namespace aetheria::local {
using namespace detail::generation;

namespace {

constexpr std::uint64_t kTerrainSalt = UINT64_C(0xF6B9371C4A28D05E);

void apply_boundary(OpenLocalSkeleton& result, spatial::BoundarySide side,
                    const spatial::BoundaryProfile& profile) {
    for (std::uint8_t position = 0; position < kLocalWidth; ++position) {
        const auto tile = boundary_tile(side, position);
        const auto index = tile_index(tile.x, tile.y);
        result.elevation[index] = profile.elevation[position];
        result.tiles.ground[index] = profile.ground[position];
        result.tiles.edges[index * kDirections + static_cast<std::size_t>(side)] =
            profile.edges[position];
    }
}

}  // namespace

OpenLocalSkeleton build_open_local_skeleton(const LocalSlowVars& slow,
                                            std::uint64_t local_seed,
                                            const rules::Ruleset& ruleset) {
    if (slow.zoning != site::SiteZoning::Open || slow.structure.has_value()) {
        throw std::invalid_argument{"路線 B 只接受開放且無建築結構的 Site tile"};
    }
    if (ruleset.ground(slow.ground) == nullptr || ruleset.feature(slow.feature) == nullptr ||
        !ruleset.wilderness_generation_rules().loaded) {
        throw std::runtime_error{"Local 路線 B 含無效慢變數或缺少資料規則"};
    }
    const auto no_edge = ruleset.find_edge("edge.none");
    const auto water_ground = ruleset.find_ground("ground.water");
    if (!no_edge.has_value() || !water_ground.has_value()) {
        throw std::runtime_error{"Local 路線 B 缺少 edge.none 或 ground.water"};
    }
    for (const auto edge : slow.edges) {
        if (ruleset.edge(edge) == nullptr) {
            throw std::runtime_error{"Local 路線 B 引用不存在的 EdgeId"};
        }
    }

    OpenLocalSkeleton result;
    result.tiles.ground.assign(kLocalTileCount, slow.ground);
    result.tiles.overlay.assign(kLocalTileCount, slow.overlay);
    result.tiles.occupant.assign(kLocalTileCount, 0);
    result.tiles.edges.assign(kLocalTileCount * kDirections, *no_edge);
    result.tiles.light.assign(kLocalTileCount, UINT8_MAX);
    result.elevation.resize(kLocalTileCount);
    result.boundaries = slow.boundaries;

    const auto& north = slow.boundaries[static_cast<std::size_t>(spatial::BoundarySide::North)];
    const auto& east = slow.boundaries[static_cast<std::size_t>(spatial::BoundarySide::East)];
    const auto& south = slow.boundaries[static_cast<std::size_t>(spatial::BoundarySide::South)];
    const auto& west = slow.boundaries[static_cast<std::size_t>(spatial::BoundarySide::West)];
    for (std::uint16_t y = 0; y < kLocalHeight; ++y) {
        for (std::uint16_t x = 0; x < kLocalWidth; ++x) {
            const auto index = tile_index(x, y);
            const auto vertical = static_cast<std::int64_t>(north.elevation[x]) +
                (static_cast<std::int64_t>(south.elevation[x]) - north.elevation[x]) * y / 63;
            const auto horizontal = static_cast<std::int64_t>(west.elevation[y]) +
                (static_cast<std::int64_t>(east.elevation[y]) - west.elevation[y]) * x / 63;
            const auto edge_distance = std::min({x, y, static_cast<std::uint16_t>(63U - x),
                                                 static_cast<std::uint16_t>(63U - y)});
            const auto noise = static_cast<std::int64_t>(
                                   worldgen::splitmix64(local_seed ^ kTerrainSalt ^ index) % 17U) -
                               8;
            result.elevation[index] = static_cast<std::uint16_t>(std::clamp<std::int64_t>(
                (vertical + horizontal) / 2 + noise * edge_distance / 31, 0, UINT16_MAX));
        }
    }
    texture_ground(result, local_seed, ruleset);
    for (std::size_t side = 0; side < slow.boundaries.size(); ++side) {
        apply_boundary(result, static_cast<spatial::BoundarySide>(side), slow.boundaries[side]);
        for (const auto& crossing : slow.boundaries[side].crossings) {
            const auto* definition = ruleset.edge(crossing.kind);
            if (definition == nullptr) {
                throw std::runtime_error{"Local 邊界 crossing 引用不存在的 EdgeDef"};
            }
            result.road_path_count +=
                static_cast<std::uint16_t>((definition->flags & rules::kEdgeRoadFlag) != 0);
            result.river_path_count +=
                static_cast<std::uint16_t>((definition->flags & rules::kEdgeRiverFlag) != 0);
        }
    }
    connect_crossings(result, *water_ground, ruleset);
    scatter(result, slow, local_seed, ruleset);
    place_objects(result, slow, local_seed, ruleset);
    if (!result.valid_layout()) {
        throw std::logic_error{"Local 路線 B 產生無效版面"};
    }
    if (const auto unresolved = count_unresolved_open_paths(result, ruleset); unresolved != 0) {
        throw std::logic_error{"Local 路線 B 產生斷頭 crossing：" +
                               std::to_string(unresolved)};
    }
    return result;
}

}  // namespace aetheria::local

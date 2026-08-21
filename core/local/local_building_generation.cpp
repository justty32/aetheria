#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "core/local/local_building_detail.h"
#include "core/local/local_buildings.h"

namespace aetheria::local {
namespace {

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

void apply_boundaries(BuildingLocalSkeleton& result, const LocalSlowVars& slow) {
    auto& ground = result.layers.at(0);
    for (std::size_t side = 0; side < slow.boundaries.size(); ++side) {
        const auto direction = static_cast<spatial::BoundarySide>(side);
        const auto& profile = slow.boundaries[side];
        for (std::uint8_t position = 0; position < kLocalWidth; ++position) {
            const auto tile = boundary_tile(direction, position);
            const auto index = detail::tile_index(tile.x, tile.y);
            result.elevation[index] = profile.elevation[position];
            ground.ground[index] = profile.ground[position];
            detail::set_edge(ground, tile, direction, profile.edges[position]);
        }
    }
}

void interpolate_foundation(BuildingLocalSkeleton& result, const LocalSlowVars& slow) {
    const auto& north = slow.boundaries[static_cast<std::size_t>(spatial::BoundarySide::North)];
    const auto& east = slow.boundaries[static_cast<std::size_t>(spatial::BoundarySide::East)];
    const auto& south = slow.boundaries[static_cast<std::size_t>(spatial::BoundarySide::South)];
    const auto& west = slow.boundaries[static_cast<std::size_t>(spatial::BoundarySide::West)];
    for (std::uint16_t y = 0; y < kLocalHeight; ++y) {
        for (std::uint16_t x = 0; x < kLocalWidth; ++x) {
            const auto vertical =
                static_cast<std::int64_t>(north.elevation[x]) +
                (static_cast<std::int64_t>(south.elevation[x]) - north.elevation[x]) * y / 63;
            const auto horizontal =
                static_cast<std::int64_t>(west.elevation[y]) +
                (static_cast<std::int64_t>(east.elevation[y]) - west.elevation[y]) * x / 63;
            result.elevation[detail::tile_index(x, y)] = static_cast<std::uint16_t>(
                std::clamp<std::int64_t>((vertical + horizontal) / 2, 0, UINT16_MAX));
        }
    }
}

}  // namespace

namespace detail {

LocalTiles make_local_layer(rules::GroundId ground, rules::EdgeId no_edge, std::uint8_t light) {
    LocalTiles result;
    result.ground.assign(kLocalTileCount, ground);
    result.overlay.assign(kLocalTileCount, OverlayId::None);
    result.occupant.assign(kLocalTileCount, 0);
    result.edges.assign(kLocalTileCount * kDirections, no_edge);
    result.light.assign(kLocalTileCount, light);
    return result;
}

void set_edge(LocalTiles& tiles, LocalXY tile, spatial::BoundarySide side, rules::EdgeId edge) {
    const auto index = tile_index(tile.x, tile.y);
    tiles.edges[index * kDirections + static_cast<std::size_t>(side)] = edge;
    std::int32_t neighbor_x = tile.x;
    std::int32_t neighbor_y = tile.y;
    auto opposite = spatial::BoundarySide::North;
    switch (side) {
        case spatial::BoundarySide::North:
            --neighbor_y;
            opposite = spatial::BoundarySide::South;
            break;
        case spatial::BoundarySide::East:
            ++neighbor_x;
            opposite = spatial::BoundarySide::West;
            break;
        case spatial::BoundarySide::South:
            ++neighbor_y;
            opposite = spatial::BoundarySide::North;
            break;
        case spatial::BoundarySide::West:
            --neighbor_x;
            opposite = spatial::BoundarySide::East;
            break;
    }
    if (neighbor_x >= 0 && neighbor_y >= 0 && neighbor_x < static_cast<std::int32_t>(kLocalWidth) &&
        neighbor_y < static_cast<std::int32_t>(kLocalHeight)) {
        const auto neighbor = tile_index(static_cast<std::uint16_t>(neighbor_x),
                                         static_cast<std::uint16_t>(neighbor_y));
        tiles.edges[neighbor * kDirections + static_cast<std::size_t>(opposite)] = edge;
    }
}

}  // namespace detail

std::size_t BuildingLocalSkeleton::entity_count() const noexcept {
    return furniture.size() + ambient_resident_count;
}

bool BuildingLocalSkeleton::valid_layout() const noexcept {
    return elevation.size() == kLocalTileCount && layers.contains(0) &&
           std::ranges::all_of(layers, [](const auto& layer) {
               return layer.first >= -1 && layer.first <= 1 && layer.second.valid_layout();
           });
}

BuildingLocalSkeleton build_building_local_skeleton(const LocalSlowVars& slow,
                                                    std::uint64_t local_seed,
                                                    const rules::Ruleset& ruleset) {
    if (slow.zoning == site::SiteZoning::Open || !slow.structure.has_value()) {
        throw std::invalid_argument{"路線 A 只接受建築分區且帶 structure 的 Site tile"};
    }
    if (ruleset.ground(slow.ground) == nullptr || ruleset.feature(slow.feature) == nullptr ||
        ruleset.building(*slow.structure) == nullptr || !ruleset.local_building_rules().loaded) {
        throw std::runtime_error{"Local 路線 A 含無效慢變數或缺少資料規則"};
    }
    const auto no_edge = ruleset.find_edge("edge.none");
    if (!no_edge.has_value()) {
        throw std::runtime_error{"Local 路線 A 缺少 edge.none"};
    }
    for (const auto edge : slow.edges) {
        if (ruleset.edge(edge) == nullptr) {
            throw std::runtime_error{"Local 路線 A 引用不存在的 EdgeId"};
        }
    }

    const auto& config = ruleset.local_building_rules();
    BuildingLocalSkeleton result;
    result.layers.emplace(0, detail::make_local_layer(config.foundation_ground, *no_edge, 224));
    result.elevation.resize(kLocalTileCount);
    result.boundaries = slow.boundaries;
    interpolate_foundation(result, slow);
    apply_boundaries(result, slow);
    detail::build_house_geometry(result, slow, local_seed, ruleset);
    detail::fill_furniture(result, local_seed, ruleset);
    if (!valid_building_invariants(result, ruleset)) {
        throw std::logic_error{"Local 路線 A 產生無效建築或封死房間"};
    }
    return result;
}

}  // namespace aetheria::local

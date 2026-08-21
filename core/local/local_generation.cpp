#include "core/local/local_tiles.h"

#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>

namespace aetheria::local {
namespace {

constexpr std::size_t kDirections = 4;
constexpr std::uint64_t kTerrainSalt = UINT64_C(0xF6B9371C4A28D05E);
constexpr std::uint64_t kGroundTextureSalt = UINT64_C(0x8E25C19A74D603BF);
constexpr std::uint64_t kScatterSalt = UINT64_C(0x31D8A6C7E50B924F);
constexpr std::uint64_t kObjectSalt = UINT64_C(0xB04E298DC6137A5F);
constexpr std::array<std::uint32_t, 2> kPathFlags{rules::kEdgeRoadFlag,
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

void paint_path_brush(OpenLocalSkeleton& result, LocalXY center, std::uint8_t width,
                      std::uint32_t flags, rules::GroundId water_ground) {
    const auto half = static_cast<std::int32_t>(width / 2U);
    for (std::int32_t dy = -half; dy <= half; ++dy) {
        for (std::int32_t dx = -half; dx <= half; ++dx) {
            const auto x = static_cast<std::int32_t>(center.x) + dx;
            const auto y = static_cast<std::int32_t>(center.y) + dy;
            if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(kLocalWidth) ||
                y >= static_cast<std::int32_t>(kLocalHeight)) {
                continue;
            }
            const auto index = tile_index(static_cast<std::uint16_t>(x),
                                          static_cast<std::uint16_t>(y));
            if ((flags & rules::kEdgeRiverFlag) != 0) {
                result.tiles.ground[index] = water_ground;
            }
            if ((flags & rules::kEdgeRoadFlag) != 0) {
                result.tiles.overlay[index] = OverlayId::Road;
            }
        }
    }
}

void paint_axis_segment(OpenLocalSkeleton& result, LocalXY first, LocalXY last,
                        std::uint8_t width, std::uint32_t flags,
                        rules::GroundId water_ground) {
    const auto dx = first.x < last.x ? 1 : (first.x > last.x ? -1 : 0);
    const auto dy = first.y < last.y ? 1 : (first.y > last.y ? -1 : 0);
    auto x = static_cast<std::int32_t>(first.x);
    auto y = static_cast<std::int32_t>(first.y);
    while (true) {
        paint_path_brush(result,
                         {static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y)},
                         width, flags, water_ground);
        if (x == last.x && y == last.y) {
            break;
        }
        x += dx;
        y += dy;
    }
}

void route_path(OpenLocalSkeleton& result, const PathAnchor& anchor, LocalXY hub,
                std::uint32_t path_flag, rules::GroundId water_ground) {
    const bool vertical_entry = anchor.side == spatial::BoundarySide::North ||
                                anchor.side == spatial::BoundarySide::South;
    const LocalXY bend = vertical_entry ? LocalXY{anchor.tile.x, hub.y}
                                        : LocalXY{hub.x, anchor.tile.y};
    paint_axis_segment(result, anchor.tile, bend, anchor.width, path_flag, water_ground);
    paint_axis_segment(result, bend, hub, anchor.width, path_flag, water_ground);
}

[[nodiscard]] std::vector<PathAnchor> collect_path_anchors(
    const OpenLocalSkeleton& skeleton, const rules::Ruleset& ruleset) {
    std::vector<PathAnchor> result;
    for (std::size_t side_index = 0; side_index < skeleton.boundaries.size(); ++side_index) {
        const auto side = static_cast<spatial::BoundarySide>(side_index);
        for (const auto& crossing : skeleton.boundaries[side_index].crossings) {
            const auto* definition = ruleset.edge(crossing.kind);
            if (definition != nullptr) {
                result.push_back(
                    {boundary_tile(side, crossing.pos), side, crossing.width, definition->flags});
            }
        }
    }
    return result;
}

void connect_crossings(OpenLocalSkeleton& result, rules::GroundId water_ground,
                       const rules::Ruleset& ruleset) {
    const auto anchors = collect_path_anchors(result, ruleset);
    for (std::size_t kind_index = 0; kind_index < kPathFlags.size(); ++kind_index) {
        const auto path_flag = kPathFlags[kind_index];
        const LocalXY hub{static_cast<std::uint16_t>(31U + kind_index),
                          static_cast<std::uint16_t>(31U + kind_index)};
        std::size_t crossing_count{};
        for (const auto& anchor : anchors) {
            if ((anchor.flags & path_flag) == 0) {
                continue;
            }
            route_path(result, anchor, hub, path_flag, water_ground);
            ++crossing_count;
        }
        if (crossing_count == 1U) {
            paint_path_brush(result, hub, 5, path_flag, water_ground);
            result.path_endpoints.push_back({hub, path_flag});
        }
    }
}

[[nodiscard]] bool path_present(const OpenLocalSkeleton& skeleton, std::size_t index,
                                std::uint32_t path_flag,
                                const rules::Ruleset& ruleset) noexcept {
    if (path_flag == rules::kEdgeRoadFlag) {
        return skeleton.tiles.overlay[index] == OverlayId::Road;
    }
    const auto* ground = ruleset.ground(skeleton.tiles.ground[index]);
    return ground != nullptr && (ground->flags & rules::kGroundWaterFlag) != 0;
}

void texture_ground(OpenLocalSkeleton& result, std::uint64_t local_seed,
                    const rules::Ruleset& ruleset) {
    const auto mud = ruleset.find_ground("ground.mud");
    const auto stone = ruleset.find_ground("ground.stone");
    if (!mud.has_value() || !stone.has_value()) {
        return;
    }
    constexpr std::uint16_t kPatchExtent = 4;
    for (std::uint16_t y = 0; y < kLocalHeight; ++y) {
        for (std::uint16_t x = 0; x < kLocalWidth; ++x) {
            const auto index = tile_index(x, y);
            const auto* base = ruleset.ground(result.tiles.ground[index]);
            if (base == nullptr || (base->flags & rules::kGroundWaterFlag) != 0) {
                continue;
            }
            const auto patch = static_cast<std::uint64_t>(y / kPatchExtent) *
                                   (kLocalWidth / kPatchExtent) +
                               x / kPatchExtent;
            const auto patch_noise =
                worldgen::splitmix64(local_seed ^ kGroundTextureSalt ^ patch);
            const auto fine_noise = worldgen::splitmix64(patch_noise ^ index);
            if (patch_noise % 100U >= 58U || fine_noise % 100U >= 46U) {
                continue;
            }
            result.tiles.ground[index] = (fine_noise >> 8U) % 5U == 0 ? *stone : *mud;
        }
    }
}

void scatter(OpenLocalSkeleton& result, const LocalSlowVars& slow, std::uint64_t local_seed,
             const rules::Ruleset& ruleset) {
    const auto& config = ruleset.wilderness_generation_rules();
    const auto* feature = ruleset.feature(slow.feature);
    const bool forest = feature != nullptr && (feature->flags & rules::kFeatureForestFlag) != 0;
    const auto density = forest ? config.forest_vegetation_percent
                                : config.sparse_vegetation_percent;
    const auto extent = config.jitter_cell_extent;
    for (std::uint16_t cell_y = 0; cell_y < kLocalHeight; cell_y += extent) {
        for (std::uint16_t cell_x = 0; cell_x < kLocalWidth; cell_x += extent) {
            const auto cell = static_cast<std::uint64_t>(cell_y) * kLocalWidth + cell_x;
            const auto seed = worldgen::splitmix64(local_seed ^ kScatterSalt ^ cell);
            if (seed % 100U >= density) {
                continue;
            }
            const auto width = std::min<std::uint16_t>(extent, kLocalWidth - cell_x);
            const auto height = std::min<std::uint16_t>(extent, kLocalHeight - cell_y);
            const auto x = static_cast<std::uint16_t>(cell_x + seed % width);
            const auto y = static_cast<std::uint16_t>(cell_y + (seed >> 16U) % height);
            const auto index = tile_index(x, y);
            const auto* ground = ruleset.ground(result.tiles.ground[index]);
            if (ground == nullptr || (ground->flags & rules::kGroundWaterFlag) != 0 ||
                result.tiles.overlay[index] != OverlayId::None) {
                continue;
            }
            result.tiles.overlay[index] =
                (seed & 3U) == 0 ? OverlayId::Stone : OverlayId::Vegetation;
            ++result.scatter_count;
        }
    }
}

void place_objects(OpenLocalSkeleton& result, const LocalSlowVars& slow,
                   std::uint64_t local_seed, const rules::Ruleset& ruleset) {
    const auto* feature = ruleset.feature(slow.feature);
    const bool mine = feature != nullptr && (feature->flags & rules::kFeatureMineFlag) != 0;
    const auto count = mine ? ruleset.wilderness_generation_rules().mine_resource_points
                            : ruleset.wilderness_generation_rules().base_resource_points;
    const auto start = static_cast<std::size_t>(
        worldgen::splitmix64(local_seed ^ kObjectSalt) % kLocalTileCount);
    for (std::size_t offset = 0;
         offset < kLocalTileCount && result.object_count < count; ++offset) {
        const auto index = (start + offset * 977U) % kLocalTileCount;
        const auto* ground = ruleset.ground(result.tiles.ground[index]);
        if (ground == nullptr || (ground->flags & rules::kGroundWaterFlag) != 0 ||
            result.tiles.overlay[index] != OverlayId::None) {
            continue;
        }
        result.tiles.overlay[index] = OverlayId::ScatteredObject;
        ++result.object_count;
    }
}

void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= UINT64_C(1099511628211);
}

template <typename Value> void hash_integer(std::uint64_t& hash, Value value) noexcept {
    const auto bits = static_cast<std::make_unsigned_t<Value>>(value);
    for (std::size_t byte = 0; byte < sizeof(bits); ++byte) {
        hash_byte(hash, static_cast<std::uint8_t>(bits >> (byte * 8U)));
    }
}

}  // namespace

bool LocalTiles::valid_layout() const noexcept {
    return ground.size() == kLocalTileCount && overlay.size() == kLocalTileCount &&
           occupant.size() == kLocalTileCount && edges.size() == kLocalTileCount * kDirections &&
           light.size() == kLocalTileCount;
}

bool LocalTiles::empty() const noexcept {
    return ground.empty() && overlay.empty() && occupant.empty() && edges.empty() && light.empty();
}

bool OpenLocalSkeleton::valid_layout() const noexcept {
    return tiles.valid_layout() && elevation.size() == kLocalTileCount &&
           std::ranges::all_of(path_endpoints, [](const LocalPathEndpoint& endpoint) {
               return endpoint.tile.x < kLocalWidth && endpoint.tile.y < kLocalHeight;
           });
}

std::size_t count_unresolved_open_paths(const OpenLocalSkeleton& skeleton,
                                        const rules::Ruleset& ruleset) {
    if (!skeleton.valid_layout()) {
        return 1;
    }
    const auto anchors = collect_path_anchors(skeleton, ruleset);
    std::size_t unresolved{};
    for (const auto path_flag : kPathFlags) {
        for (std::size_t anchor_index = 0; anchor_index < anchors.size(); ++anchor_index) {
            if ((anchors[anchor_index].flags & path_flag) == 0) {
                continue;
            }
            std::array<bool, kLocalTileCount> visited{};
            std::array<std::size_t, kLocalTileCount> frontier{};
            std::size_t head{};
            std::size_t tail{};
            const auto start = tile_index(anchors[anchor_index].tile.x,
                                          anchors[anchor_index].tile.y);
            if (!path_present(skeleton, start, path_flag, ruleset)) {
                ++unresolved;
                continue;
            }
            frontier[tail++] = start;
            visited[start] = true;
            while (head < tail) {
                const auto current = frontier[head++];
                const auto x = static_cast<std::uint16_t>(current % kLocalWidth);
                const auto y = static_cast<std::uint16_t>(current / kLocalWidth);
                const std::array<LocalXY, 4> neighbors{
                    LocalXY{static_cast<std::uint16_t>(x > 0 ? x - 1U : x), y},
                    LocalXY{static_cast<std::uint16_t>(x + 1U < kLocalWidth ? x + 1U : x), y},
                    LocalXY{x, static_cast<std::uint16_t>(y > 0 ? y - 1U : y)},
                    LocalXY{x,
                            static_cast<std::uint16_t>(y + 1U < kLocalHeight ? y + 1U : y)}};
                for (const auto neighbor : neighbors) {
                    const auto next = tile_index(neighbor.x, neighbor.y);
                    if (!visited[next] && path_present(skeleton, next, path_flag, ruleset)) {
                        visited[next] = true;
                        frontier[tail++] = next;
                    }
                }
            }
            const bool reaches_other = std::ranges::any_of(
                anchors, [&](const PathAnchor& other) {
                    return &other != &anchors[anchor_index] && (other.flags & path_flag) != 0 &&
                           visited[tile_index(other.tile.x, other.tile.y)];
                });
            const bool reaches_endpoint = std::ranges::any_of(
                skeleton.path_endpoints, [&](const LocalPathEndpoint& endpoint) {
                    return (endpoint.edge_flags & path_flag) != 0 &&
                           visited[tile_index(endpoint.tile.x, endpoint.tile.y)];
                });
            unresolved += static_cast<std::size_t>(!reaches_other && !reaches_endpoint);
        }
    }
    return unresolved;
}

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

std::uint64_t hash_open_local_skeleton(const OpenLocalSkeleton& skeleton) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    hash_integer(hash, static_cast<std::uint64_t>(skeleton.tiles.ground.size()));
    for (const auto value : skeleton.tiles.ground) {
        hash_integer(hash, rules::value_of(value));
    }
    for (const auto value : skeleton.tiles.overlay) {
        hash_integer(hash, static_cast<std::uint16_t>(value));
    }
    for (const auto value : skeleton.tiles.occupant) {
        hash_integer(hash, value);
    }
    for (const auto value : skeleton.tiles.edges) {
        hash_integer(hash, rules::value_of(value));
    }
    for (const auto value : skeleton.tiles.light) {
        hash_byte(hash, value);
    }
    for (const auto value : skeleton.elevation) {
        hash_integer(hash, value);
    }
    for (const auto& endpoint : skeleton.path_endpoints) {
        hash_integer(hash, endpoint.tile.x);
        hash_integer(hash, endpoint.tile.y);
        hash_integer(hash, endpoint.edge_flags);
    }
    return hash;
}

}  // namespace aetheria::local

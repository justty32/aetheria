// local_generation_validation.cpp：Local 版面、開放路徑可達性與骨架雜湊。

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

[[nodiscard]] bool path_present(const OpenLocalSkeleton& skeleton, std::size_t index,
                                std::uint32_t path_flag,
                                const rules::Ruleset& ruleset) noexcept {
    if (path_flag == rules::kEdgeRoadFlag) {
        return skeleton.tiles.overlay[index] == OverlayId::Road;
    }
    const auto* ground = ruleset.ground(skeleton.tiles.ground[index]);
    return ground != nullptr && (ground->flags & rules::kGroundWaterFlag) != 0;
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

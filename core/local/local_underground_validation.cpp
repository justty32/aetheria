// local_underground_validation.cpp：路線 C 的 flood-fill 可達性與不變式。

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/local/local_underground.h"
#include "core/local/local_underground_detail.h"

namespace aetheria::local {
namespace {

struct Reachability {
    std::map<std::int8_t, std::vector<std::uint8_t>> visited;
    std::size_t reachable_tiles{};
};

[[nodiscard]] bool passable(rules::EdgeId edge, const rules::Ruleset& ruleset) noexcept {
    const auto* definition = ruleset.edge(edge);
    if (definition == nullptr) {
        return false;
    }
    const bool wall = (definition->flags & rules::kEdgeWallFlag) != 0;
    const bool openable = (definition->flags & rules::kEdgeOpenableFlag) != 0;
    return !wall || openable;
}

[[nodiscard]] Reachability flood(const UndergroundLocalSkeleton& skeleton,
                                 const rules::Ruleset& ruleset) {
    Reachability result;
    for (const auto& [z, mask] : skeleton.excavated) {
        result.visited.emplace(z, std::vector<std::uint8_t>(mask.size()));
    }
    if (!skeleton.excavated.contains(-1) ||
        skeleton.excavated.at(-1).at(underground_detail::tile_index(skeleton.entrance)) == 0) {
        return result;
    }
    std::deque<std::pair<std::int8_t, LocalXY>> pending{{-1, skeleton.entrance}};
    result.visited.at(-1)[underground_detail::tile_index(skeleton.entrance)] = 1;
    constexpr std::array<std::int32_t, 4> dx{0, 1, 0, -1};
    constexpr std::array<std::int32_t, 4> dy{-1, 0, 1, 0};
    while (!pending.empty()) {
        const auto [z, tile] = pending.front();
        pending.pop_front();
        ++result.reachable_tiles;
        const auto& tiles = skeleton.layers.at(z);
        for (std::size_t side = 0; side < dx.size(); ++side) {
            const auto x = static_cast<std::int32_t>(tile.x) + dx[side];
            const auto y = static_cast<std::int32_t>(tile.y) + dy[side];
            if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(kLocalWidth) ||
                y >= static_cast<std::int32_t>(kLocalHeight) ||
                !passable(tiles.edges[underground_detail::tile_index(tile) * 4U + side], ruleset)) {
                continue;
            }
            const LocalXY neighbor{static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y)};
            const auto index = underground_detail::tile_index(neighbor);
            if (skeleton.excavated.at(z)[index] == 0 || result.visited.at(z)[index] != 0) {
                continue;
            }
            result.visited.at(z)[index] = 1;
            pending.emplace_back(z, neighbor);
        }
        for (const auto& link : skeleton.vertical_links) {
            if (link.tile != tile || (link.upper_z != z && link.lower_z != z)) {
                continue;
            }
            const auto destination = link.upper_z == z ? link.lower_z : link.upper_z;
            if (destination >= 0 || !result.visited.contains(destination)) {
                continue;
            }
            const auto index = underground_detail::tile_index(tile);
            if (skeleton.excavated.at(destination)[index] != 0 &&
                result.visited.at(destination)[index] == 0) {
                result.visited.at(destination)[index] = 1;
                pending.emplace_back(destination, tile);
            }
        }
    }
    return result;
}

[[nodiscard]] bool symmetric_edges(const LocalTiles& tiles) noexcept {
    for (std::uint16_t y = 0; y < kLocalHeight; ++y) {
        for (std::uint16_t x = 0; x < kLocalWidth; ++x) {
            const auto base = underground_detail::tile_index({x, y}) * 4U;
            if (x + 1U < kLocalWidth) {
                const auto east =
                    underground_detail::tile_index({static_cast<std::uint16_t>(x + 1U), y}) * 4U;
                if (tiles.edges[base + 1U] != tiles.edges[east + 3U]) {
                    return false;
                }
            }
            if (y + 1U < kLocalHeight) {
                const auto south =
                    underground_detail::tile_index({x, static_cast<std::uint16_t>(y + 1U)}) * 4U;
                if (tiles.edges[base + 2U] != tiles.edges[south]) {
                    return false;
                }
            }
        }
    }
    return true;
}

}  // namespace

std::size_t count_unreachable_underground_rooms(const UndergroundLocalSkeleton& skeleton,
                                                const rules::Ruleset& ruleset) {
    const auto reachable = flood(skeleton, ruleset);
    return std::ranges::count_if(skeleton.rooms, [&](const UndergroundRoom& room) {
        const LocalXY center{
            static_cast<std::uint16_t>(room.footprint.x + room.footprint.width / 2U),
            static_cast<std::uint16_t>(room.footprint.y + room.footprint.height / 2U)};
        const auto layer = reachable.visited.find(room.z);
        return layer == reachable.visited.end() ||
               layer->second.at(underground_detail::tile_index(center)) == 0;
    });
}

bool all_underground_tiles_reachable(const UndergroundLocalSkeleton& skeleton,
                                     const rules::Ruleset& ruleset) {
    const auto reachable = flood(skeleton, ruleset);
    return reachable.reachable_tiles == skeleton.excavated_count;
}

bool valid_underground_invariants(const UndergroundLocalSkeleton& skeleton,
                                  const rules::Ruleset& ruleset) noexcept {
    try {
        if (!skeleton.valid_layout() || !all_underground_tiles_reachable(skeleton, ruleset) ||
            count_unreachable_underground_rooms(skeleton, ruleset) != 0 ||
            !std::ranges::all_of(skeleton.layers, [&](const auto& layer) {
                return symmetric_edges(layer.second);
            })) {
            return false;
        }
        for (const auto& room : skeleton.rooms) {
            if (room.z >= 0 || !skeleton.layers.contains(room.z) || room.footprint.width == 0 ||
                room.footprint.height == 0 ||
                room.footprint.x + room.footprint.width > kLocalWidth ||
                room.footprint.y + room.footprint.height > kLocalHeight) {
                return false;
            }
        }
        for (const auto& link : skeleton.vertical_links) {
            const auto index = underground_detail::tile_index(link.tile);
            if (link.lower_z != link.upper_z - 1 || link.lower_z >= 0 ||
                !skeleton.layers.contains(link.upper_z) ||
                !skeleton.layers.contains(link.lower_z) ||
                skeleton.layers.at(link.upper_z).overlay[index] != OverlayId::Stairs ||
                skeleton.layers.at(link.lower_z).overlay[index] != OverlayId::Stairs ||
                skeleton.excavated.at(link.lower_z)[index] == 0 ||
                (link.upper_z < 0 && skeleton.excavated.at(link.upper_z)[index] == 0)) {
                return false;
            }
        }
        if (skeleton.kind == rules::UndergroundKind::Mine && !skeleton.rooms.empty()) {
            return false;
        }
        if (skeleton.kind == rules::UndergroundKind::Dungeon &&
            skeleton.rooms.size() < static_cast<std::size_t>(skeleton.depth) * 2U) {
            return false;
        }
        if (skeleton.kind == rules::UndergroundKind::Ruin) {
            if (skeleton.ruin_original_segments == 0) {
                return false;
            }
            const auto permille = static_cast<std::uint64_t>(skeleton.ruin_removed_segments) *
                                  1000U / skeleton.ruin_original_segments;
            if (permille < 600U || permille > 800U) {
                return false;
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace aetheria::local

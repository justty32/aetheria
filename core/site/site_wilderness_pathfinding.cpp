#include "core/site/site_wilderness_detail.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <queue>
#include <stdexcept>
#include <vector>

namespace aetheria::site::wilderness_detail {
namespace {

struct QueueNode {
    std::uint32_t score{};
    std::uint16_t index{};
};

struct QueueLater {
    [[nodiscard]] bool operator()(const QueueNode& left, const QueueNode& right) const noexcept {
        return left.score > right.score ||
               (left.score == right.score && left.index > right.index);
    }
};

[[nodiscard]] std::uint32_t manhattan(SiteXY first, SiteXY second) noexcept {
    return static_cast<std::uint32_t>(
        std::abs(static_cast<std::int32_t>(first.x) - second.x) +
        std::abs(static_cast<std::int32_t>(first.y) - second.y));
}

}  // namespace

std::vector<SiteXY> find_path(const SiteSkeleton& terrain, SiteXY start, SiteXY goal,
                              bool avoid_water) {
    constexpr auto unreachable = std::numeric_limits<std::uint32_t>::max();
    std::array<std::uint32_t, kSiteTileCount> costs{};
    std::array<std::uint16_t, kSiteTileCount> parents{};
    costs.fill(unreachable);
    parents.fill(UINT16_MAX);
    const auto start_index = static_cast<std::uint16_t>(tile_index(start));
    const auto goal_index = static_cast<std::uint16_t>(tile_index(goal));
    std::priority_queue<QueueNode, std::vector<QueueNode>, QueueLater> open;
    costs[start_index] = 0;
    open.push({manhattan(start, goal), start_index});
    constexpr std::array<std::int16_t, 4> dx{0, 1, 0, -1};
    constexpr std::array<std::int16_t, 4> dy{-1, 0, 1, 0};

    while (!open.empty()) {
        const auto current = open.top();
        open.pop();
        if (current.index == goal_index) {
            break;
        }
        const SiteXY tile{static_cast<std::uint16_t>(current.index % kSiteWidth),
                          static_cast<std::uint16_t>(current.index / kSiteWidth)};
        for (std::size_t direction = 0; direction < dx.size(); ++direction) {
            const auto x = static_cast<std::int32_t>(tile.x) + dx[direction];
            const auto y = static_cast<std::int32_t>(tile.y) + dy[direction];
            if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(kSiteWidth) ||
                y >= static_cast<std::int32_t>(kSiteHeight)) {
                continue;
            }
            const SiteXY next{static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y)};
            const bool boundary = next.x == 0 || next.y == 0 || next.x == kSiteWidth - 1U ||
                                  next.y == kSiteHeight - 1U;
            if (boundary && next != goal) {
                continue;
            }
            const auto next_index = static_cast<std::uint16_t>(tile_index(next));
            const auto slope = static_cast<std::uint32_t>(std::abs(
                static_cast<std::int32_t>(terrain.elevation[current.index]) -
                terrain.elevation[next_index]));
            const auto water_penalty =
                avoid_water ? static_cast<std::uint32_t>(terrain.water[next_index]) * 96U : 0U;
            const auto step = 1U + slope + water_penalty;
            if (costs[current.index] == unreachable ||
                costs[current.index] > unreachable - step) {
                continue;
            }
            const auto candidate = costs[current.index] + step;
            if (candidate < costs[next_index]) {
                costs[next_index] = candidate;
                parents[next_index] = current.index;
                open.push({candidate + manhattan(next, goal), next_index});
            }
        }
    }
    if (costs[goal_index] == unreachable) {
        throw std::runtime_error{"荒野有界 A* 找不到 crossing 間路徑"};
    }
    std::vector<SiteXY> result;
    for (auto cursor = goal_index;; cursor = parents[cursor]) {
        result.push_back({static_cast<std::uint16_t>(cursor % kSiteWidth),
                          static_cast<std::uint16_t>(cursor / kSiteWidth)});
        if (cursor == start_index) {
            break;
        }
    }
    std::ranges::reverse(result);
    return result;
}

}  // namespace aetheria::site::wilderness_detail

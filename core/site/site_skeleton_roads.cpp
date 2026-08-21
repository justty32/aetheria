#include "core/site/site_skeleton_detail.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <queue>
#include <stdexcept>
#include <vector>

namespace aetheria::site::detail {
namespace {

struct FrontierNode {
  std::uint32_t cost{};
  std::uint16_t index{};
};

struct GreaterNode {
  [[nodiscard]] bool operator()(const FrontierNode &left,
                                const FrontierNode &right) const noexcept {
    return left.cost > right.cost ||
           (left.cost == right.cost && left.index > right.index);
  }
};

[[nodiscard]] SiteXY boundary_tile(SiteBoundarySide side,
                                   std::uint16_t position) noexcept {
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

[[nodiscard]] std::uint32_t manhattan(SiteXY left, SiteXY right) noexcept {
  const auto dx = std::abs(static_cast<std::int32_t>(left.x) - right.x);
  const auto dy = std::abs(static_cast<std::int32_t>(left.y) - right.y);
  return static_cast<std::uint32_t>(dx + dy);
}

void connect_edge(SiteSkeleton &skeleton, std::size_t from, std::size_t to,
                  rules::EdgeId road) {
  const auto from_x = static_cast<std::uint16_t>(from % kSiteWidth);
  const auto from_y = static_cast<std::uint16_t>(from / kSiteWidth);
  const auto to_x = static_cast<std::uint16_t>(to % kSiteWidth);
  const auto to_y = static_cast<std::uint16_t>(to / kSiteWidth);
  if (to_y + 1U == from_y) {
    skeleton.edges[from * kDirections] = road;
    skeleton.edges[to * kDirections + 2U] = road;
  } else if (to_x == from_x + 1U) {
    skeleton.edges[from * kDirections + 1U] = road;
    skeleton.edges[to * kDirections + 3U] = road;
  } else if (to_y == from_y + 1U) {
    skeleton.edges[from * kDirections + 2U] = road;
    skeleton.edges[to * kDirections] = road;
  } else if (to_x + 1U == from_x) {
    skeleton.edges[from * kDirections + 3U] = road;
    skeleton.edges[to * kDirections + 1U] = road;
  }
}

void route_to_center(SiteSkeleton &skeleton, SiteXY start, SiteXY goal,
                     rules::EdgeId road) {
  constexpr auto kUnknown = std::numeric_limits<std::uint32_t>::max();
  constexpr std::array<std::array<std::int16_t, 2>, 4> offsets{
      {{{0, -1}}, {{1, 0}}, {{0, 1}}, {{-1, 0}}}};
  std::vector<std::uint32_t> cost(kSiteTileCount, kUnknown);
  std::vector<std::int32_t> parent(kSiteTileCount, -1);
  std::vector<std::uint8_t> closed(kSiteTileCount);
  std::priority_queue<FrontierNode, std::vector<FrontierNode>, GreaterNode>
      frontier;
  const auto start_index = tile_index(start);
  const auto goal_index = tile_index(goal);
  cost[start_index] = 0;
  frontier.push(
      {manhattan(start, goal), static_cast<std::uint16_t>(start_index)});

  while (!frontier.empty()) {
    const auto current = frontier.top();
    frontier.pop();
    if (closed[current.index] != 0) {
      continue;
    }
    closed[current.index] = UINT8_C(1);
    if (current.index == goal_index) {
      break;
    }
    const auto x = static_cast<std::uint16_t>(current.index % kSiteWidth);
    const auto y = static_cast<std::uint16_t>(current.index / kSiteWidth);
    for (const auto &offset : offsets) {
      const auto nx = static_cast<std::int32_t>(x) + offset[0];
      const auto ny = static_cast<std::int32_t>(y) + offset[1];
      if (nx < 0 || ny < 0 || nx >= static_cast<std::int32_t>(kSiteWidth) ||
          ny >= static_cast<std::int32_t>(kSiteHeight)) {
        continue;
      }
      const auto next = tile_index(static_cast<std::uint16_t>(nx),
                                   static_cast<std::uint16_t>(ny));
      const auto step = (skeleton.roads[next] != 0 ? 2U : 20U) +
                        static_cast<std::uint32_t>(local_slope(
                            skeleton, static_cast<std::uint16_t>(nx),
                            static_cast<std::uint16_t>(ny))) *
                            2U +
                        (skeleton.water[next] != 0 ? 500U : 0U);
      const auto candidate = cost[current.index] + step;
      if (candidate >= cost[next]) {
        continue;
      }
      cost[next] = candidate;
      parent[next] = current.index;
      const SiteXY next_tile{static_cast<std::uint16_t>(nx),
                             static_cast<std::uint16_t>(ny)};
      frontier.push({candidate + manhattan(next_tile, goal) * 2U,
                     static_cast<std::uint16_t>(next)});
    }
  }
  if (cost[goal_index] == kUnknown) {
    throw std::runtime_error{"Site 主幹道 A* 找不到城心"};
  }
  for (auto current = goal_index;;) {
    skeleton.roads[current] = UINT8_C(1);
    if (current == start_index) {
      break;
    }
    const auto previous = static_cast<std::size_t>(parent[current]);
    connect_edge(skeleton, previous, current, road);
    current = previous;
  }
}

} // namespace

void generate_site_roads(SiteSkeleton &skeleton, const SiteSlowVars &slow,
                         std::uint64_t site_seed,
                         const rules::Ruleset &ruleset) {
  for (std::size_t index = 0; index < slow.edges.size(); ++index) {
    const auto *edge = ruleset.edge(slow.edges[index]);
    if ((edge->flags & rules::kEdgeRoadFlag) == 0) {
      continue;
    }
    const auto side = static_cast<SiteBoundarySide>(index);
    skeleton.gates.push_back(
        {side, boundary_tile(side, boundary_position(site_seed, side)),
         slow.edges[index]});
  }
  skeleton.city_center = choose_city_center(skeleton);
  const auto road = ruleset.find_edge("edge.road");
  if (!road.has_value()) {
    throw std::runtime_error{"Site 主幹道生成缺少 edge.road"};
  }
  for (const auto &gate : skeleton.gates) {
    route_to_center(skeleton, gate.tile, skeleton.city_center, *road);
  }
}

} // namespace aetheria::site::detail

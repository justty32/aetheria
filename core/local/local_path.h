#pragma once

// local_path.h：Local 執行期的邊感知整數 A*，以及未載入目標的粗路徑退化。

#include "core/local/local_navigation.h"
#include "core/rules/ruleset.h"

#include <aetheria/runtime/cross_zone.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <tuple>
#include <utility>
#include <vector>

namespace aetheria::local {

// z 是同一 Local zone 的 LocalPayload::layers key；水平位址沿用 LocalLocation。
struct LocalPathLocation {
  LocalLocation horizontal{};
  std::int8_t z{};

  constexpr auto
  operator<=>(const LocalPathLocation &) const noexcept = default;
};

// 門位址必須含 z，避免不同樓層同一 xy 邊共用門狀態。
struct LocalPathEdgeAddress {
  LocalPathLocation first{};
  LocalPathLocation second{};

  constexpr auto
  operator<=>(const LocalPathEdgeAddress &) const noexcept = default;
};

enum class LocalPathStatus : std::uint8_t {
  Found,
  Coarse,
  NoPath,
  Unknown,
};

enum class LocalPathInteraction : std::uint8_t {
  None,
  OpenDoor,
};

struct LocalPathStep {
  LocalPathLocation location{};
  LocalPathInteraction interaction{LocalPathInteraction::None};

  constexpr bool operator==(const LocalPathStep &) const noexcept = default;
};

struct LocalPathResult {
  LocalPathStatus status{LocalPathStatus::Unknown};
  std::vector<LocalPathStep> steps;
  // Coarse 時由上層 Site 規劃器回傳依序經過的 Local zone；精確格路徑留空。
  std::vector<zone::ZoneKey> coarse_zones;
  std::int64_t cost{};
  std::size_t expanded_tiles{};

  bool operator==(const LocalPathResult &) const = default;
};

using LocalPathTileQuery =
    std::function<std::optional<runtime::TileView>(LocalPathLocation)>;
using LocalPathEdgeQuery = std::function<std::optional<runtime::EdgeView>(
    LocalPathLocation, spatial::BoundarySide)>;
using LocalPathVerticalQuery =
    std::function<std::vector<std::int8_t>(LocalPathLocation)>;
using LocalPathDoorStateQuery =
    std::function<DoorState(const LocalPathEdgeAddress &)>;
using LocalCoarsePathQuery =
    std::function<std::optional<std::vector<zone::ZoneKey>>(zone::ZoneKey,
                                                            zone::ZoneKey)>;

// 查詢 callback 都是唯讀。vertical_neighbors 只可回同格的相鄰
// z；實作會排序去重。
struct LocalPathQueries {
  LocalPathTileQuery peek_tile;
  LocalPathEdgeQuery peek_edge;
  LocalPathVerticalQuery vertical_neighbors;
  LocalPathDoorStateQuery door_states;
  LocalCoarsePathQuery coarse_path;
  std::size_t max_expanded_tiles{100'000};
};

namespace path_detail {

struct PathRecord {
  std::int64_t cost{std::numeric_limits<std::int64_t>::max()};
  std::optional<LocalPathLocation> parent;
  LocalPathInteraction interaction{LocalPathInteraction::None};
};

[[nodiscard]] inline LocalPathResult unknown_result() { return {}; }

[[nodiscard]] inline LocalPathResult
coarse_or_unknown(const LocalPathQueries &queries, zone::ZoneKey start,
                  zone::ZoneKey goal) {
  if (!queries.coarse_path) {
    return unknown_result();
  }
  auto coarse = queries.coarse_path(start, goal);
  if (!coarse.has_value() || coarse->empty()) {
    return unknown_result();
  }
  LocalPathResult result;
  result.status = LocalPathStatus::Coarse;
  result.coarse_zones = std::move(*coarse);
  return result;
}

[[nodiscard]] inline std::optional<std::int32_t>
ground_cost(const LocalPathQueries &queries, const rules::Ruleset &ruleset,
            LocalPathLocation location) {
  if (!queries.peek_tile) {
    return std::nullopt;
  }
  const auto tile = queries.peek_tile(location);
  if (!tile.has_value()) {
    return std::nullopt;
  }
  const auto *definition = ruleset.ground(tile->ground);
  if (definition == nullptr) {
    return std::nullopt;
  }
  return definition->move_cost;
}

[[nodiscard]] inline std::int32_t
minimum_ground_cost(const rules::Ruleset &ruleset) noexcept {
  auto minimum = std::numeric_limits<std::int32_t>::max();
  for (const auto &ground : ruleset.grounds()) {
    if (ground.move_cost > 0) {
      minimum = std::min(minimum, ground.move_cost);
    }
  }
  return minimum == std::numeric_limits<std::int32_t>::max() ? 0 : minimum;
}

[[nodiscard]] inline std::int64_t
heuristic(LocalPathLocation from, LocalPathLocation goal,
          std::int32_t minimum_cost) noexcept {
  const auto global_x = [](LocalPathLocation location) {
    return static_cast<std::int64_t>(
               zone::local_x_of(location.horizontal.zone)) *
               kLocalWidth +
           location.horizontal.tile.x;
  };
  const auto global_y = [](LocalPathLocation location) {
    return static_cast<std::int64_t>(
               zone::local_y_of(location.horizontal.zone)) *
               kLocalHeight +
           location.horizontal.tile.y;
  };
  const auto distance = std::llabs(global_x(from) - global_x(goal)) +
                        std::llabs(global_y(from) - global_y(goal)) +
                        std::llabs(static_cast<std::int64_t>(from.z) - goal.z);
  return distance * minimum_cost;
}

[[nodiscard]] inline LocalPathEdgeAddress
edge_address(LocalPathLocation first, LocalPathLocation second) noexcept {
  if (second < first) {
    std::swap(first, second);
  }
  return {first, second};
}

[[nodiscard]] inline DoorState door_state(const LocalPathQueries &queries,
                                          const LocalPathEdgeAddress &edge) {
  return queries.door_states ? queries.door_states(edge) : DoorState::Closed;
}

struct Transition {
  LocalPathLocation destination{};
  std::int32_t cost{};
  LocalPathInteraction interaction{LocalPathInteraction::None};
};

[[nodiscard]] inline std::optional<Transition>
horizontal_transition(const LocalPathQueries &queries,
                      const rules::Ruleset &ruleset, LocalPathLocation from,
                      spatial::BoundarySide direction) {
  const auto adjacent = adjacent_location(from.horizontal, direction);
  if (!adjacent.has_value() || !queries.peek_edge) {
    return std::nullopt;
  }
  const LocalPathLocation destination{*adjacent, from.z};
  const auto cost = ground_cost(queries, ruleset, destination);
  const auto edge = queries.peek_edge(from, direction);
  if (!cost.has_value() || *cost <= 0 || !edge.has_value()) {
    return std::nullopt;
  }
  const auto *definition = ruleset.edge(edge->edge);
  if (definition == nullptr) {
    return std::nullopt;
  }
  if ((definition->flags & rules::kEdgeWallFlag) == 0) {
    return Transition{destination, *cost, LocalPathInteraction::None};
  }
  if ((definition->flags & rules::kEdgeOpenableFlag) == 0) {
    return std::nullopt;
  }
  switch (door_state(queries, edge_address(from, destination))) {
  case DoorState::Open:
    return Transition{destination, *cost, LocalPathInteraction::None};
  case DoorState::Closed:
    return Transition{destination, *cost, LocalPathInteraction::OpenDoor};
  case DoorState::Locked:
    return std::nullopt;
  }
  return std::nullopt;
}

inline void relax(
    const Transition &transition, LocalPathLocation from,
    LocalPathLocation goal, std::int32_t minimum_cost, std::int64_t known_cost,
    std::map<LocalPathLocation, PathRecord> &records,
    std::priority_queue<
        std::tuple<std::int64_t, std::int64_t, std::int64_t, LocalPathLocation>,
        std::vector<std::tuple<std::int64_t, std::int64_t, std::int64_t,
                               LocalPathLocation>>,
        std::greater<>> &open) {
  if (known_cost > std::numeric_limits<std::int64_t>::max() - transition.cost) {
    return;
  }
  const auto candidate = known_cost + transition.cost;
  auto [found, inserted] = records.try_emplace(transition.destination);
  static_cast<void>(inserted);
  if (candidate >= found->second.cost) {
    return;
  }
  found->second = {candidate, from, transition.interaction};
  const auto remaining = heuristic(transition.destination, goal, minimum_cost);
  open.emplace(candidate + remaining, remaining, candidate,
               transition.destination);
}

[[nodiscard]] inline LocalPathResult
reconstruct(const std::map<LocalPathLocation, PathRecord> &records,
            LocalPathLocation start, LocalPathLocation goal,
            std::size_t expanded_tiles) {
  const auto goal_record = records.find(goal);
  if (goal_record == records.end()) {
    return unknown_result();
  }
  LocalPathResult result;
  result.status = LocalPathStatus::Found;
  result.cost = goal_record->second.cost;
  result.expanded_tiles = expanded_tiles;
  auto current = goal;
  while (true) {
    const auto found = records.find(current);
    if (found == records.end()) {
      return unknown_result();
    }
    result.steps.push_back({current, found->second.interaction});
    if (current == start) {
      break;
    }
    if (!found->second.parent.has_value()) {
      return unknown_result();
    }
    current = *found->second.parent;
  }
  std::ranges::reverse(result.steps);
  return result;
}

} // namespace path_detail

// 精確路徑只遍歷 callback 能讀到的格。目標未載入或跨 Site 時才退化到粗路徑。
[[nodiscard]] inline LocalPathResult
find_local_path(const LocalPathQueries &queries, const rules::Ruleset &ruleset,
                LocalPathLocation start, LocalPathLocation goal) {
  const auto start_cost = path_detail::ground_cost(queries, ruleset, start);
  if (!start_cost.has_value()) {
    return path_detail::unknown_result();
  }
  if (*start_cost <= 0) {
    LocalPathResult result;
    result.status = LocalPathStatus::NoPath;
    return result;
  }
  if (zone::parent_of(start.horizontal.zone) !=
      zone::parent_of(goal.horizontal.zone)) {
    return path_detail::coarse_or_unknown(queries, start.horizontal.zone,
                                          goal.horizontal.zone);
  }
  const auto goal_cost = path_detail::ground_cost(queries, ruleset, goal);
  if (!goal_cost.has_value()) {
    return path_detail::coarse_or_unknown(queries, start.horizontal.zone,
                                          goal.horizontal.zone);
  }
  if (*goal_cost <= 0) {
    LocalPathResult result;
    result.status = LocalPathStatus::NoPath;
    return result;
  }
  const auto minimum_cost = path_detail::minimum_ground_cost(ruleset);
  if (minimum_cost <= 0 || queries.max_expanded_tiles == 0U) {
    return path_detail::unknown_result();
  }

  // 同 f 時先取較小 h，沿等成本輪廓朝目標前進；最後以完整位址穩定裁決。
  using Candidate =
      std::tuple<std::int64_t, std::int64_t, std::int64_t, LocalPathLocation>;
  std::priority_queue<Candidate, std::vector<Candidate>, std::greater<>> open;
  std::map<LocalPathLocation, path_detail::PathRecord> records;
  records.emplace(start, path_detail::PathRecord{0, std::nullopt,
                                                 LocalPathInteraction::None});
  const auto start_heuristic =
      path_detail::heuristic(start, goal, minimum_cost);
  open.emplace(start_heuristic, start_heuristic, 0, start);

  constexpr std::array directions{
      spatial::BoundarySide::North, spatial::BoundarySide::East,
      spatial::BoundarySide::South, spatial::BoundarySide::West};
  std::size_t expanded{};
  while (!open.empty()) {
    const auto [estimate, remaining, known_cost, current] = open.top();
    static_cast<void>(estimate);
    static_cast<void>(remaining);
    open.pop();
    const auto found = records.find(current);
    if (found == records.end() || found->second.cost != known_cost) {
      continue;
    }
    ++expanded;
    if (current == goal) {
      return path_detail::reconstruct(records, start, goal, expanded);
    }
    if (expanded >= queries.max_expanded_tiles) {
      auto result = path_detail::unknown_result();
      result.expanded_tiles = expanded;
      return result;
    }

    for (const auto direction : directions) {
      const auto transition = path_detail::horizontal_transition(
          queries, ruleset, current, direction);
      if (transition.has_value()) {
        path_detail::relax(*transition, current, goal, minimum_cost, known_cost,
                           records, open);
      }
    }

    if (queries.vertical_neighbors) {
      auto layers = queries.vertical_neighbors(current);
      std::ranges::sort(layers);
      const auto unique = std::ranges::unique(layers);
      layers.erase(unique.begin(), unique.end());
      for (const auto z : layers) {
        if (std::abs(static_cast<int>(z) - static_cast<int>(current.z)) != 1) {
          continue;
        }
        const LocalPathLocation destination{current.horizontal, z};
        const auto cost =
            path_detail::ground_cost(queries, ruleset, destination);
        if (!cost.has_value() || *cost <= 0) {
          continue;
        }
        path_detail::relax({destination, *cost, LocalPathInteraction::None},
                           current, goal, minimum_cost, known_cost, records,
                           open);
      }
    }
  }

  LocalPathResult result;
  result.status = LocalPathStatus::NoPath;
  result.expanded_tiles = expanded;
  return result;
}

// 現有 CrossZoneRuntime 只公開 z=0；M5.19 可改用上面的 callback
// 入口供多層查詢。
[[nodiscard]] inline LocalPathQueries
make_ground_path_queries(const runtime::CrossZoneRuntime &runtime,
                         const DoorStateQuery &door_states = {},
                         LocalCoarsePathQuery coarse_path = {}) {
  LocalPathQueries result;
  result.peek_tile = [&runtime](LocalPathLocation location) {
    return location.z == 0 ? runtime.peek_tile(location.horizontal.zone,
                                               location.horizontal.tile)
                           : std::optional<runtime::TileView>{};
  };
  result.peek_edge = [&runtime](LocalPathLocation location,
                                spatial::BoundarySide direction) {
    return location.z == 0
               ? runtime.peek_edge(location.horizontal.zone,
                                   location.horizontal.tile, direction)
               : std::optional<runtime::EdgeView>{};
  };
  result.door_states = [door_states](const LocalPathEdgeAddress &edge) {
    return query_door_state(door_states,
                            {edge.first.horizontal < edge.second.horizontal
                                 ? edge.first.horizontal
                                 : edge.second.horizontal,
                             edge.first.horizontal < edge.second.horizontal
                                 ? edge.second.horizontal
                                 : edge.first.horizontal});
  };
  result.coarse_path = std::move(coarse_path);
  return result;
}

[[nodiscard]] inline LocalPathResult
find_local_path(const runtime::CrossZoneRuntime &runtime,
                const rules::Ruleset &ruleset, LocalLocation start,
                LocalLocation goal, const DoorStateQuery &door_states = {},
                LocalCoarsePathQuery coarse_path = {}) {
  const auto queries =
      make_ground_path_queries(runtime, door_states, std::move(coarse_path));
  return find_local_path(queries, ruleset, {start, 0}, {goal, 0});
}

} // namespace aetheria::local

#pragma once

// local_path_impl.h：A* 搜尋與地表查詢轉接；由 local_path.h 引入。

namespace aetheria::local {

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

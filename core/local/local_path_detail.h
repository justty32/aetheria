#pragma once

// local_path_detail.h：A* 成本、轉移與重建 helper；由 local_path.h 引入。

namespace aetheria::local {

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

} // namespace aetheria::local

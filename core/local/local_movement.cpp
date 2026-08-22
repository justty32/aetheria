// local_movement.cpp：探索一步只讀取目的格與跨越邊，回傳門互動或阻擋結果。

#include "core/local/local_movement.h"

#include <aetheria/runtime/cross_zone.h>

#include "core/rules/ruleset.h"

namespace aetheria::local {

ExplorationStepResult
assess_exploration_step(const runtime::CrossZoneRuntime &runtime,
                        const rules::Ruleset &ruleset, LocalLocation from,
                        spatial::BoundarySide direction,
                        const DoorStateQuery &door_states) {
  const auto destination = adjacent_location(from, direction);
  if (!destination.has_value() ||
      !runtime.peek_tile(destination->zone, destination->tile).has_value()) {
    return ExplorationStepResult::Unknown;
  }
  const auto edge = runtime.peek_edge(from.zone, from.tile, direction);
  if (!edge.has_value()) {
    return ExplorationStepResult::Unknown;
  }
  const auto *definition = ruleset.edge(edge->edge);
  if (definition == nullptr) {
    return ExplorationStepResult::Unknown;
  }
  if ((definition->flags & rules::kEdgeWallFlag) == 0) {
    return ExplorationStepResult::Allowed;
  }
  if ((definition->flags & rules::kEdgeOpenableFlag) == 0) {
    return ExplorationStepResult::BlockedByWall;
  }
  const auto address = local_edge_address(from, direction);
  if (!address.has_value()) {
    return ExplorationStepResult::Unknown;
  }
  switch (query_door_state(door_states, *address)) {
  case DoorState::Open:
    return ExplorationStepResult::Allowed;
  case DoorState::Closed:
    return ExplorationStepResult::MustOpenDoor;
  case DoorState::Locked:
    return ExplorationStepResult::BlockedByLockedDoor;
  }
  return ExplorationStepResult::Unknown;
}

} // namespace aetheria::local

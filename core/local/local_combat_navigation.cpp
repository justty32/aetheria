// local_combat_navigation.cpp：邊界部署、敵軍感知與回合移動。

#include "core/local/local_combat_detail.h"

#include "core/base/check.h"
#include "core/rules/ruleset.h"

#include <algorithm>
#include <ranges>

#include "core/local/local_fov.h"
#include "core/local/local_movement.h"
#include "core/local/local_path.h"

#include <aetheria/runtime/cross_zone.h>

#include <cstdlib>
#include <limits>

namespace aetheria::local {
namespace {

[[nodiscard]] constexpr LocalXY
retreat_goal(LocalXY from, spatial::BoundarySide side) noexcept {
  switch (side) {
  case spatial::BoundarySide::North:
    return {from.x, 0};
  case spatial::BoundarySide::East:
    return {static_cast<std::uint16_t>(kLocalWidth - 1U), from.y};
  case spatial::BoundarySide::South:
    return {from.x, static_cast<std::uint16_t>(kLocalHeight - 1U)};
  case spatial::BoundarySide::West:
    return {0, from.y};
  }
  return from;
}

[[nodiscard]] constexpr LocalXY boundary_tile(spatial::BoundarySide side,
                                              std::uint16_t slot,
                                              std::uint16_t depth) noexcept {
  switch (side) {
  case spatial::BoundarySide::North:
    return {slot, depth};
  case spatial::BoundarySide::East:
    return {static_cast<std::uint16_t>(kLocalWidth - 1U - depth), slot};
  case spatial::BoundarySide::South:
    return {slot, static_cast<std::uint16_t>(kLocalHeight - 1U - depth)};
  case spatial::BoundarySide::West:
    return {depth, slot};
  }
  return {};
}

[[nodiscard]] std::optional<spatial::BoundarySide>
direction_between(LocalLocation from, LocalLocation to) noexcept {
  constexpr std::array directions{
      spatial::BoundarySide::North, spatial::BoundarySide::East,
      spatial::BoundarySide::South, spatial::BoundarySide::West};
  for (const auto direction : directions) {
    if (adjacent_location(from, direction) == to) {
      return direction;
    }
  }
  return std::nullopt;
}

} // namespace

void deploy_local_combatants(LocalCombatState &state, zone::ZoneKey battlefield,
                             const SiteCombatBoundaryCondition &boundary) {
  std::array<std::uint16_t, 2> counts{};
  for (auto &unit : state.combatants) {
    AETH_CHECK(unit.unit_id > 0);
    const auto index = side_index(unit.side);
    const auto ordinal = counts[index]++;
    AETH_CHECK(ordinal < kLocalTileCount / 8U);
    const auto slot = static_cast<std::uint16_t>(ordinal % kLocalWidth);
    const auto depth = static_cast<std::uint16_t>(ordinal / kLocalWidth);
    unit.location = {battlefield,
                     boundary_tile(boundary.entry(unit.side), slot, depth)};
  }
}

namespace detail::combat {

[[nodiscard]] std::size_t move_toward(LocalCombatant &actor, LocalLocation goal,
                                      const runtime::CrossZoneRuntime &runtime,
                                      const rules::Ruleset &ruleset,
                                      bool stop_adjacent) {
  const auto path = find_local_path(runtime, ruleset, actor.location, goal);
  if (path.status != LocalPathStatus::Found || path.steps.empty()) {
    return 0;
  }
  auto maximum_index = path.steps.size() - 1U;
  if (stop_adjacent && maximum_index > 0U) {
    --maximum_index;
  }
  maximum_index = std::min(maximum_index, kCombatMovementTiles);
  std::size_t moved{};
  for (std::size_t index = 1; index <= maximum_index; ++index) {
    const auto next = path.steps[index].location.horizontal;
    const auto direction = direction_between(actor.location, next);
    if (!direction.has_value() ||
        assess_exploration_step(runtime, ruleset, actor.location, *direction) !=
            ExplorationStepResult::Allowed) {
      break;
    }
    actor.location = next;
    ++moved;
  }
  return moved;
}

[[nodiscard]] std::int64_t distance(LocalLocation first,
                                    LocalLocation second) noexcept {
  if (first.zone != second.zone) {
    return std::numeric_limits<std::int64_t>::max();
  }
  return std::llabs(static_cast<std::int64_t>(first.tile.x) - second.tile.x) +
         std::llabs(static_cast<std::int64_t>(first.tile.y) - second.tile.y);
}

[[nodiscard]] const LocalCombatant *
nearest_enemy(const LocalCombatState &state,
              const LocalCombatant &actor) noexcept {
  const LocalCombatant *result{};
  auto best = std::numeric_limits<std::int64_t>::max();
  for (const auto &candidate : state.combatants) {
    if (!candidate.alive || candidate.side == actor.side) {
      continue;
    }
    const auto candidate_distance =
        distance(actor.location, candidate.location);
    if (candidate_distance < best ||
        (candidate_distance == best && result != nullptr &&
         candidate.unit_id < result->unit_id)) {
      result = &candidate;
      best = candidate_distance;
    }
  }
  return result;
}

[[nodiscard]] bool can_see(const LocalCombatant &actor,
                           const LocalCombatant &target,
                           const runtime::CrossZoneRuntime &runtime,
                           const rules::Ruleset &ruleset) {
  const auto fov = calculate_fov(runtime, ruleset, actor.location,
                                 {actor.stats.vision, actor.stats.vision});
  return is_visible(fov, target.location);
}

void move_combatants(
    LocalCombatState &state, const runtime::CrossZoneRuntime &runtime,
    const rules::Ruleset &ruleset, const SiteCombatBoundaryCondition &boundary,
    const LocalCombatRoundInput &input, LocalCombatRoundResult &result) {
  for (auto &actor : state.combatants) {
    if (!actor.alive) {
      continue;
    }
    const auto index = side_index(actor.side);
    if (input.retreat_requested[index]) {
      const auto goal = LocalLocation{
          actor.location.zone,
          retreat_goal(actor.location.tile, boundary.retreat(actor.side))};
      result.moved_tiles[index] +=
          move_toward(actor, goal, runtime, ruleset, false);
      continue;
    }
    const auto *target = nearest_enemy(state, actor);
    if (target == nullptr || !can_see(actor, *target, runtime, ruleset)) {
      continue;
    }
    result.moved_tiles[index] +=
        move_toward(actor, target->location, runtime, ruleset, true);
  }

}

} // namespace detail::combat
} // namespace aetheria::local

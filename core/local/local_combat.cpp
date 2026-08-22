// local_combat.cpp：以 M5 導航與 M6.1 d100 推進 L3 單位回合，再歸約到
// Site／Region。

#include "core/local/local_combat.h"

#include "core/base/check.h"
#include "core/local/local_fov.h"
#include "core/local/local_movement.h"
#include "core/local/local_path.h"
#include "core/rules/check.h"
#include "core/rules/ruleset.h"

#include <aetheria/runtime/cross_zone.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <random>
#include <ranges>
#include <utility>

namespace aetheria::local {
namespace {

constexpr std::int32_t kPermyriad = 10'000;

[[nodiscard]] constexpr LocalCombatSide
opposite(LocalCombatSide side) noexcept {
  return side == LocalCombatSide::A ? LocalCombatSide::B : LocalCombatSide::A;
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

[[nodiscard]] constexpr bool on_boundary(LocalXY tile,
                                         spatial::BoundarySide side) noexcept {
  switch (side) {
  case spatial::BoundarySide::North:
    return tile.y == 0;
  case spatial::BoundarySide::East:
    return tile.x == kLocalWidth - 1U;
  case spatial::BoundarySide::South:
    return tile.y == kLocalHeight - 1U;
  case spatial::BoundarySide::West:
    return tile.x == 0;
  }
  return false;
}

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

[[nodiscard]] std::int64_t distance(LocalLocation first,
                                    LocalLocation second) noexcept {
  if (first.zone != second.zone) {
    return std::numeric_limits<std::int64_t>::max();
  }
  return std::llabs(static_cast<std::int64_t>(first.tile.x) - second.tile.x) +
         std::llabs(static_cast<std::int64_t>(first.tile.y) - second.tile.y);
}

[[nodiscard]] LocalCombatant *find_unit(LocalCombatState &state,
                                        std::uint64_t unit_id) noexcept {
  const auto found =
      std::ranges::find(state.combatants, unit_id, &LocalCombatant::unit_id);
  return found == state.combatants.end() ? nullptr : &*found;
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

[[nodiscard]] bool can_see(const LocalCombatant &actor,
                           const LocalCombatant &target,
                           const runtime::CrossZoneRuntime &runtime,
                           const rules::Ruleset &ruleset) {
  const auto fov = calculate_fov(runtime, ruleset, actor.location,
                                 {actor.stats.vision, actor.stats.vision});
  return is_visible(fov, target.location);
}

struct PendingAttack {
  std::uint64_t attacker{};
  std::uint64_t defender{};
  LocalCombatSide side{LocalCombatSide::A};
  rules::CheckResult check{};
  std::int32_t damage{};
};

void subtract_shared_morale(SharedMorale &morale,
                            std::int32_t penalty) noexcept {
  morale.local = std::max(0, morale.local - penalty);
  morale.site = std::max(0, morale.site - penalty);
  morale.region = std::max(0, morale.region - penalty);
}

[[nodiscard]] std::int32_t alive_count(const LocalCombatState &state,
                                       LocalCombatSide side) noexcept {
  return static_cast<std::int32_t>(std::ranges::count_if(
      state.combatants, [side](const LocalCombatant &unit) {
        return unit.side == side && unit.alive;
      }));
}

void mark_routed(LocalCombatState &state, LocalCombatSide side) noexcept {
  auto &layers = state.layers[side_index(side)];
  layers.local = LayerCombatState::Routed;
  layers.site = LayerCombatState::Routed;
  layers.region = LayerCombatState::Routed;
  layers.region_retreat_direction.reset();
  layers.routed_remnant = RoutedRemnantCohort{alive_count(state, side),
                                              world::Significance::Region};
}

[[nodiscard]] std::int32_t
apply_micro_variation(std::int32_t site_loss, std::int32_t power,
                      std::int32_t balance) noexcept {
  AETH_CHECK(site_loss >= 0);
  AETH_CHECK(power > 0);
  const auto delta = static_cast<std::int64_t>(site_loss) * balance / 500;
  return static_cast<std::int32_t>(std::clamp<std::int64_t>(
      static_cast<std::int64_t>(site_loss) + delta, 0, power));
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

void apply_local_morale_event(LocalCombatState &state, LocalCombatSide side,
                              LocalMoraleEvent event) noexcept {
  std::int32_t penalty{};
  switch (event) {
  case LocalMoraleEvent::CommanderKilled:
    penalty = 25;
    break;
  case LocalMoraleEvent::BannerCaptured:
    penalty = 15;
    break;
  case LocalMoraleEvent::AlliesRouted:
    penalty = 30;
    break;
  }
  subtract_shared_morale(state.morale[side_index(side)], penalty);
}

LocalCombatRoundResult resolve_local_combat_round(
    LocalCombatState &state, const runtime::CrossZoneRuntime &runtime,
    const rules::Ruleset &ruleset, const SiteCombatBoundaryCondition &boundary,
    const LocalCombatRoundInput &input, std::uint64_t seed) {
  for (const auto power : input.side_power) {
    AETH_CHECK(power > 0);
  }
  for (std::size_t index = 0; index < input.side_power.size(); ++index) {
    AETH_CHECK(input.site_expected_loss[index] >= 0);
    AETH_CHECK(input.site_expected_loss[index] <= input.side_power[index]);
  }

  LocalCombatRoundResult result;
  std::ranges::sort(state.combatants, {}, &LocalCombatant::unit_id);

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

  // 相鄰 seed 共用 d100 序列並反轉單位取骰順序，讓對稱交戰形成
  // antithetic pair；每一個檢定仍只走 M6.1 perform_check 一次。
  std::mt19937_64 rng{(seed >> 1U) ^
                      static_cast<std::uint64_t>(state.round_index)};
  std::vector<PendingAttack> pending;
  std::vector<const LocalCombatant *> attack_order;
  attack_order.reserve(state.combatants.size());
  for (const auto &actor : state.combatants) {
    attack_order.push_back(&actor);
  }
  if ((seed & 1U) != 0U) {
    std::ranges::reverse(attack_order);
  }
  for (const auto *actor_pointer : attack_order) {
    const auto &actor = *actor_pointer;
    if (!actor.alive) {
      continue;
    }
    const auto *target = nearest_enemy(state, actor);
    if (target == nullptr || distance(actor.location, target->location) != 1 ||
        !can_see(actor, *target, runtime, ruleset)) {
      continue;
    }
    const auto check =
        rules::perform_check(rng, actor.stats.accuracy, -target->stats.evasion,
                             0, ruleset.check_rules());
    const auto damage =
        check.success
            ? std::max(1, actor.stats.damage * check.effect_percent / 100)
            : 0;
    pending.push_back(
        {actor.unit_id, target->unit_id, actor.side, check, damage});
    ++result.attacks[side_index(actor.side)];
    if (check.success) {
      ++result.hits[side_index(actor.side)];
    }
  }

  std::array<std::int32_t, 2> effect_score{};
  for (const auto &attack : pending) {
    effect_score[side_index(attack.side)] += attack.check.effect_percent;
    auto *target = find_unit(state, attack.defender);
    if (target == nullptr || !target->alive || attack.damage <= 0) {
      continue;
    }
    target->health = std::max(0, target->health - attack.damage);
    if (target->health == 0) {
      target->alive = false;
      if (target->commander) {
        apply_local_morale_event(state, target->side,
                                 LocalMoraleEvent::CommanderKilled);
      }
      if (target->banner) {
        apply_local_morale_event(state, target->side,
                                 LocalMoraleEvent::BannerCaptured);
      }
    }
  }

  const auto balance = effect_score[side_index(LocalCombatSide::A)] -
                       effect_score[side_index(LocalCombatSide::B)];
  result.loss[side_index(LocalCombatSide::A)] = apply_micro_variation(
      input.site_expected_loss[side_index(LocalCombatSide::A)],
      input.side_power[side_index(LocalCombatSide::A)], -balance);
  result.loss[side_index(LocalCombatSide::B)] = apply_micro_variation(
      input.site_expected_loss[side_index(LocalCombatSide::B)],
      input.side_power[side_index(LocalCombatSide::B)], balance);

  for (const auto &action : input.personal_actions) {
    const auto *actor = find_unit(state, action.unit_id);
    AETH_CHECK(actor != nullptr);
    const auto target_side = opposite(actor->side);
    const auto target_index = side_index(target_side);
    const auto allocation = world::apply_personal_contribution(
        result.loss[target_index], input.side_power[target_index],
        {actor->significance, action.requested_loss_shift});
    result.loss[target_index] = allocation.final_loss;
    result.contributions.push_back({actor->unit_id, actor->side, allocation});
  }

  for (const auto side : {LocalCombatSide::A, LocalCombatSide::B}) {
    const auto index = side_index(side);
    const auto retreat_succeeded =
        input.retreat_requested[index] &&
        std::ranges::any_of(state.combatants, [&](const LocalCombatant &unit) {
          return unit.side == side && unit.alive &&
                 on_boundary(unit.location.tile, boundary.retreat(side));
        });
    if (retreat_succeeded) {
      auto &layers = state.layers[index];
      layers.local = LayerCombatState::Retreated;
      layers.site = LayerCombatState::Retreated;
      layers.region = LayerCombatState::Retreated;
      layers.region_retreat_direction = boundary.retreat(side);
      const auto pursuer = opposite(side);
      const auto pursuer_index = side_index(pursuer);
      if (input.pursue_retreat[pursuer_index]) {
        state.layers[pursuer_index].local = LayerCombatState::Pursuing;
        state.layers[pursuer_index].site = LayerCombatState::Pursuing;
        state.layers[pursuer_index].region = LayerCombatState::Pursuing;
        const auto remaining = input.side_power[index] - result.loss[index];
        result.pursuit_loss[index] = static_cast<std::int32_t>(
            static_cast<std::int64_t>(remaining) *
            ruleset.combat_rules().pursuit_loss_permyriad / kPermyriad);
        result.loss[index] =
            std::min(input.side_power[index],
                     result.loss[index] + result.pursuit_loss[index]);
      }
    }

    if (state.morale[index].local <= 0) {
      mark_routed(state, side);
    } else if (alive_count(state, side) == 0 &&
               state.layers[index].local != LayerCombatState::Retreated) {
      state.layers[index].local = LayerCombatState::Annihilated;
      state.layers[index].site = LayerCombatState::Annihilated;
      state.layers[index].region = LayerCombatState::Annihilated;
    }
  }

  ++state.round_index;
  result.layers = state.layers;
  return result;
}

} // namespace aetheria::local

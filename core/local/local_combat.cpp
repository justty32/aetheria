// local_combat.cpp：逐單位攻擊、傷亡與戰鬥結果結算。

#include "core/local/local_combat_detail.h"

#include "core/base/check.h"
#include "core/rules/ruleset.h"

#include <algorithm>
#include <random>
#include <ranges>

namespace aetheria::local {
namespace {

constexpr std::int32_t kPermyriad = 10'000;

struct PendingAttack {
  std::uint64_t attacker{};
  std::uint64_t defender{};
  LocalCombatSide side{LocalCombatSide::A};
  rules::CheckResult check{};
  std::int32_t damage{};
};

[[nodiscard]] constexpr LocalCombatSide
opposite(LocalCombatSide side) noexcept {
  return side == LocalCombatSide::A ? LocalCombatSide::B : LocalCombatSide::A;
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

} // namespace

using namespace detail::combat;

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

  move_combatants(state, runtime, ruleset, boundary, input, result);

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

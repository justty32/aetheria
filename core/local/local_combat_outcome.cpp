// local_combat_outcome.cpp：士氣事件、潰散與戰損歸約。

#include "core/local/local_combat_detail.h"

#include "core/base/check.h"
#include "core/rules/ruleset.h"

#include <algorithm>
#include <ranges>

namespace aetheria::local {
namespace {

void subtract_shared_morale(SharedMorale &morale,
                            std::int32_t penalty) noexcept {
  morale.local = std::max(0, morale.local - penalty);
  morale.site = std::max(0, morale.site - penalty);
  morale.region = std::max(0, morale.region - penalty);
}

} // namespace

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

namespace detail::combat {

[[nodiscard]] LocalCombatant *find_unit(LocalCombatState &state,
                                        std::uint64_t unit_id) noexcept {
  const auto found =
      std::ranges::find(state.combatants, unit_id, &LocalCombatant::unit_id);
  return found == state.combatants.end() ? nullptr : &*found;
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

} // namespace detail::combat
} // namespace aetheria::local

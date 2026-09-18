#pragma once

// local_combat_detail.h：戰鬥回合各階段共用的內部宣告。

#include "core/local/local_combat.h"
#include "core/rules/check.h"

namespace aetheria::local::detail::combat {

[[nodiscard]] std::int64_t distance(LocalLocation first,
                                    LocalLocation second) noexcept;

[[nodiscard]] LocalCombatant *find_unit(LocalCombatState &state,
                                        std::uint64_t unit_id) noexcept;

[[nodiscard]] const LocalCombatant *
nearest_enemy(const LocalCombatState &state,
              const LocalCombatant &actor) noexcept;

[[nodiscard]] bool can_see(const LocalCombatant &actor,
                           const LocalCombatant &target,
                           const runtime::CrossZoneRuntime &runtime,
                           const rules::Ruleset &ruleset);

[[nodiscard]] std::size_t move_toward(LocalCombatant &actor, LocalLocation goal,
                                      const runtime::CrossZoneRuntime &runtime,
                                      const rules::Ruleset &ruleset,
                                      bool stop_adjacent);

[[nodiscard]] std::int32_t alive_count(const LocalCombatState &state,
                                       LocalCombatSide side) noexcept;

void mark_routed(LocalCombatState &state, LocalCombatSide side) noexcept;

[[nodiscard]] std::int32_t
apply_micro_variation(std::int32_t site_loss, std::int32_t power,
                      std::int32_t balance) noexcept;

void move_combatants(
    LocalCombatState &state, const runtime::CrossZoneRuntime &runtime,
    const rules::Ruleset &ruleset, const SiteCombatBoundaryCondition &boundary,
    const LocalCombatRoundInput &input, LocalCombatRoundResult &result);

} // namespace aetheria::local::detail::combat

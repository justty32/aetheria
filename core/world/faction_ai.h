#pragma once

// 世界層勢力 AI 協調：observer 場強分級、共用戰鬥公式轉接與玩家同型命令執行。

#include <aetheria/ai/faction_ai.h>

#include "core/rules/ruleset.h"
#include "core/time/tick.h"
#include "core/world/diplomacy.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace aetheria::world {

struct FactionAttention {
    std::int32_t observer_strength{};
    std::int32_t travel_cost{};
    bool known{};
    bool marked{};
    bool at_war{};
};

struct FactionAiTurnReport {
    ai::FactionAiLod lod{ai::FactionAiLod::Statistical};
    ai::FactionDecision decision;
    ai::PowerChange power_change;
    std::size_t utility_evaluations{};
};

[[nodiscard]] ai::FactionAiLod classify_faction_ai_lod(
    FactionAttention attention,
    const rules::CivilizationRules::FactionAiRules& rules) noexcept;

[[nodiscard]] ai::BattleAssessment forecast_region_battle(
    std::int32_t own_estimated_power, std::int32_t target_estimated_power,
    const rules::CombatRules& rules) noexcept;

[[nodiscard]] FactionAiTurnReport plan_faction_ai_xun(
    WorldDiplomacyState& world, FactionId faction, FactionAttention attention,
    time::Tick now, const rules::Ruleset& ruleset);

void execute_faction_command(WorldDiplomacyState& world,
                             const ai::FactionCommand& command,
                             ai::PowerChange power_change, time::Tick now,
                             const rules::Ruleset& ruleset);

[[nodiscard]] FactionAiTurnReport advance_faction_ai_xun(
    WorldDiplomacyState& world, FactionId faction, FactionAttention attention,
    time::Tick now, const rules::Ruleset& ruleset);

[[nodiscard]] std::vector<FactionAiTurnReport> advance_all_factions_ai_xun(
    WorldDiplomacyState& world, std::span<const FactionAttention> attention_by_id,
    time::Tick now, const rules::Ruleset& ruleset);

void set_managed_faction_goal(WorldDiplomacyState& world, FactionId faction,
                              ai::FactionGoal goal);

void answer_defensive_alliance_call(WorldDiplomacyState& world, FactionId ally,
                                    FactionId defended, FactionId attacker,
                                    bool joins, time::Tick now,
                                    const rules::Ruleset& ruleset);

} // namespace aetheria::world

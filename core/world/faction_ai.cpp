// 勢力 AI 世界轉接；決策只看 FactionView，命令由玩家與 AI 共用此執行入口。

#include "core/world/faction_ai.h"

#include "core/observer/field.h"
#include "core/rules/combat.h"

#include <algorithm>
#include <stdexcept>

namespace aetheria::world {
namespace {

[[nodiscard]] const rules::CivilizationRules::FactionAiRules::FactionDef&
definition_for(const rules::Ruleset& ruleset, FactionId faction) {
    const auto value = static_cast<std::uint16_t>(faction);
    const auto& definitions = ruleset.civilization_rules().faction_ai.definitions;
    const auto found = std::ranges::find(definitions, value,
                                         &rules::CivilizationRules::FactionAiRules::FactionDef::faction);
    if (found == definitions.end()) {
        throw std::invalid_argument{"勢力沒有 FactionDef"};
    }
    return *found;
}

[[nodiscard]] ai::FactionPersonality personality_for(
    const rules::CivilizationRules::FactionAiRules::FactionDef& definition) noexcept {
    return {definition.expansion, definition.aggression, definition.fidelity,
            definition.commerce, definition.piety, definition.caution,
            definition.resentment};
}

[[nodiscard]] rules::CombatModifiers neutral(const rules::CombatRules& rules) noexcept {
    return {rules.modifier_scale, rules.modifier_scale, rules.modifier_scale,
            rules.modifier_scale, rules.modifier_scale};
}

[[nodiscard]] ai::BattleAssessment battle_callback(std::int32_t own,
                                                   std::int32_t target,
                                                   const void* context) noexcept {
    return forecast_region_battle(own, target,
                                  *static_cast<const rules::CombatRules*>(context));
}

} // namespace

ai::FactionAiLod classify_faction_ai_lod(
    FactionAttention attention,
    const rules::CivilizationRules::FactionAiRules& rules) noexcept {
    auto strength = attention.observer_strength;
    if (attention.marked) {
        strength = std::max(
            strength, observer::strength_for_score(rules.marked_observer_strength,
                                                   attention.travel_cost));
    }
    if (attention.at_war) {
        strength = std::max(
            strength, observer::strength_for_score(rules.war_observer_strength,
                                                   attention.travel_cost));
    }
    const auto score = observer::field_score(strength, attention.travel_cost);
    if ((attention.known || attention.marked || attention.at_war) &&
        score >= rules.full_ai_field_threshold) {
        return ai::FactionAiLod::Full;
    }
    return attention.known ? ai::FactionAiLod::Simplified
                           : ai::FactionAiLod::Statistical;
}

ai::BattleAssessment forecast_region_battle(std::int32_t own_estimated_power,
                                            std::int32_t target_estimated_power,
                                            const rules::CombatRules& rules) noexcept {
    if (own_estimated_power <= 0 || target_estimated_power <= 0) {
        return ai::BattleAssessment::Disengagement;
    }
    const auto result = rules::resolve_region_combat(
        {{own_estimated_power, neutral(rules), {}, 0},
         {target_estimated_power, neutral(rules), {}, 0},
         rules.default_exponent,
         1},
        rules);
    if (result.outcome == rules::Outcome::SideBRouted) {
        return ai::BattleAssessment::LikelyWin;
    }
    if (result.outcome == rules::Outcome::SideARouted) {
        return ai::BattleAssessment::LikelyLoss;
    }
    return ai::BattleAssessment::Disengagement;
}

FactionAiTurnReport plan_faction_ai_xun(WorldDiplomacyState& world,
                                        FactionId faction,
                                        FactionAttention attention,
                                        time::Tick now,
                                        const rules::Ruleset& ruleset) {
    const auto view = make_faction_view(world, faction);
    const auto& faction_rules = ruleset.civilization_rules().faction_ai;
    const auto lod = classify_faction_ai_lod(attention, faction_rules);
    auto& mind = world.faction_mind(faction);
    const auto personality = personality_for(definition_for(ruleset, faction));
    ai::FactionDecision decision;
    switch (lod) {
    case ai::FactionAiLod::Full:
        decision = ai::decide_full(
            view, personality,
            {.world_seed = world.persistent_state().world_seed,
             .xun = static_cast<std::int64_t>(now) / static_cast<std::int64_t>(time::kXun),
             .switch_threshold = faction_rules.goal_switch_threshold},
            mind, {battle_callback, &ruleset.combat_rules()});
        break;
    case ai::FactionAiLod::Simplified:
        decision = ai::decide_simplified(view, personality, mind);
        break;
    case ai::FactionAiLod::Statistical:
        decision = ai::decide_statistical(view, mind);
        break;
    }
    const auto truth = world.faction_truth(faction);
    if (!truth.has_value()) {
        throw std::logic_error{"勢力 AI 缺少自己的國力真值"};
    }
    const auto xun = static_cast<std::int64_t>(now) /
                     static_cast<std::int64_t>(time::kXun);
    const auto change = ai::faction_power_change(
        decision.command.kind, lod, truth->military_power, truth->economic_power,
        world.persistent_state().world_seed, xun,
        static_cast<ai::FactionKey>(static_cast<std::uint16_t>(faction)));
    const auto evaluations = lod == ai::FactionAiLod::Statistical
                                 ? 0U
                                 : (lod == ai::FactionAiLod::Simplified
                                        ? 3U
                                        : 6U + decision.scored_actions.size());
    return {lod, std::move(decision), change, evaluations};
}

FactionAiTurnReport advance_faction_ai_xun(WorldDiplomacyState& world,
                                           FactionId faction,
                                           FactionAttention attention,
                                           time::Tick now,
                                           const rules::Ruleset& ruleset) {
    auto report = plan_faction_ai_xun(world, faction, attention, now, ruleset);
    execute_faction_command(world, report.decision.command, report.power_change, now,
                            ruleset);
    return report;
}

std::vector<FactionAiTurnReport> advance_all_factions_ai_xun(
    WorldDiplomacyState& world, std::span<const FactionAttention> attention_by_id,
    time::Tick now, const rules::Ruleset& ruleset) {
    if (attention_by_id.size() != static_cast<std::size_t>(world.faction_count()) + 1U) {
        throw std::invalid_argument{"勢力 AI 場強表必須以 faction_id 為下標"};
    }
    std::vector<FactionAiTurnReport> reports;
    reports.reserve(world.faction_count());
    for (std::uint16_t faction = 1; faction <= world.faction_count(); ++faction) {
        reports.push_back(advance_faction_ai_xun(
            world, FactionId{faction}, attention_by_id[faction], now, ruleset));
    }
    return reports;
}

void set_managed_faction_goal(WorldDiplomacyState& world, FactionId faction,
                              ai::FactionGoal goal) {
    world.faction_mind(faction).forced_goal = goal;
}

} // namespace aetheria::world

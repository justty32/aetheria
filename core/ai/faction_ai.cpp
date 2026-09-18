// 勢力 AI 的目標慣性、候選篩選、整數效用評分與三級成本分離。

#include <aetheria/ai/faction_ai.h>

#include "faction_ai_detail.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace aetheria::ai {
using detail::saturating_score;

namespace {

[[nodiscard]] std::int32_t target_balance_score(const FactionView& view,
                                                const FactionEstimate& target) noexcept {
    const auto own = std::max<std::int64_t>(
        1, static_cast<std::int64_t>(view.own_military_power()) +
               view.own_economic_power());
    const auto target_power = static_cast<std::int64_t>(target.military_power) +
                              target.economic_power;
    return saturating_score(target_power * 300 / own);
}

void add_domestic(std::vector<ScoredFactionAction>& actions, const FactionView& view,
                  FactionActionKind kind, std::int32_t score) {
    actions.push_back({{kind, view.observer(), 0}, score});
}

void add_targeted(std::vector<ScoredFactionAction>& actions, const FactionView& view,
                  FactionActionKind kind, FactionKey target, std::int32_t score) {
    actions.push_back({{kind, view.observer(), target}, score});
}

} // namespace

BattleAssessment BattleEvaluator::operator()(std::int32_t own_power,
                                             std::int32_t target_power) const noexcept {
    return function == nullptr ? BattleAssessment::Disengagement
                               : function(own_power, target_power, context);
}

ScoredFactionAction
select_highest_utility(std::span<const ScoredFactionAction> actions) noexcept {
    if (actions.empty()) {
        return {};
    }
    auto best = actions.front();
    for (const auto& action : actions.subspan(1)) {
        if (action.utility > best.utility ||
            (action.utility == best.utility &&
             (action.command.target < best.command.target ||
              (action.command.target == best.command.target &&
               action.command.kind < best.command.kind)))) {
            best = action;
        }
    }
    return best;
}

FactionDecision decide_full(const FactionView& view,
                            const FactionPersonality& personality,
                            const GoalSelectionInput& goal_input,
                            FactionMindState& state, BattleEvaluator battle) {
    const auto goal = update_faction_goal(view, personality, goal_input, state);
    std::vector<ScoredFactionAction> actions;
    switch (goal) {
    case FactionGoal::Colonize:
        add_domestic(actions, view, FactionActionKind::Develop,
                     personality.commerce * 5 + personality.expansion * 2);
        for (const auto& target : view.estimates()) {
            add_targeted(actions, view, FactionActionKind::Expand, target.faction,
                         personality.expansion * 8 - target.relation.fear / 20 -
                             target.route_cost * 5);
        }
        break;
    case FactionGoal::Conquer:
    case FactionGoal::HolyWar:
        add_domestic(actions, view, FactionActionKind::Prepare,
                     personality.aggression * 4 + personality.caution * 3);
        for (const auto& target : view.estimates()) {
            if (target.military_power <= 0) {
                continue;
            }
            const auto assessment = battle(view.own_military_power(), target.military_power);
            const auto forecast = assessment == BattleAssessment::LikelyWin ? 500 : -500;
            const auto faith = goal == FactionGoal::HolyWar ? personality.piety * 4 : 0;
            const auto utility = personality.aggression * 8 + personality.resentment * 3 +
                                 target.relation.grievance / 10 - personality.caution * 5 +
                                 target_balance_score(view, target) + forecast + faith -
                                 target.route_cost * 3;
            add_targeted(actions, view, FactionActionKind::DeclareWar, target.faction,
                         saturating_score(utility));
        }
        break;
    case FactionGoal::Prosper:
        add_domestic(actions, view, FactionActionKind::Develop,
                     personality.commerce * 10 + personality.caution * 2);
        break;
    case FactionGoal::Ally:
        for (const auto& target : view.estimates()) {
            add_targeted(actions, view, FactionActionKind::OfferAlliance, target.faction,
                         personality.fidelity * 8 + personality.caution * 3 +
                             target.relation.trust / 10 + target_balance_score(view, target));
        }
        if (actions.empty()) {
            add_domestic(actions, view, FactionActionKind::Develop, personality.commerce * 4);
        }
        break;
    case FactionGoal::Survive:
        add_domestic(actions, view, FactionActionKind::Prepare,
                     personality.caution * 10 + personality.aggression);
        for (const auto& target : view.estimates()) {
            add_targeted(actions, view, FactionActionKind::PayTribute, target.faction,
                         personality.caution * 8 + target_balance_score(view, target) -
                             personality.resentment * 3);
        }
        break;
    }
    const auto winner = select_highest_utility(actions);
    return {goal, winner.command, std::move(actions)};
}

FactionDecision decide_simplified(const FactionView& view,
                                  const FactionPersonality& personality,
                                  FactionMindState& state) {
    std::vector<ScoredFactionAction> actions{
        {{FactionActionKind::Develop, view.observer(), 0}, personality.commerce * 8},
        {{FactionActionKind::Prepare, view.observer(), 0},
         personality.aggression * 5 + personality.caution * 4},
        {{FactionActionKind::Expand, view.observer(), 0}, personality.expansion * 8},
    };
    const auto winner = select_highest_utility(actions);
    if (!state.initialized) {
        state.goal = FactionGoal::Prosper;
        state.initialized = true;
    }
    return {state.goal, winner.command, std::move(actions)};
}

FactionDecision decide_statistical(const FactionView& view,
                                   FactionMindState& state) noexcept {
    if (!state.initialized) {
        state.goal = FactionGoal::Prosper;
        state.initialized = true;
    }
    return {state.goal, {FactionActionKind::StatisticalProgress, view.observer(), 0}, {}};
}

} // namespace aetheria::ai

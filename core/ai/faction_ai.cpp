// 勢力 AI 的目標慣性、候選篩選、整數效用評分與三級成本分離。

#include <aetheria/ai/faction_ai.h>

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace aetheria::ai {
namespace {

[[nodiscard]] constexpr std::uint64_t mix64(std::uint64_t value) noexcept {
    value += UINT64_C(0x9e3779b97f4a7c15);
    value = (value ^ (value >> 30U)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31U);
}

[[nodiscard]] std::int32_t saturating_score(std::int64_t value) noexcept {
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(
        value, std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::max()));
}

[[nodiscard]] std::int32_t situation_noise(const GoalSelectionInput& input,
                                           FactionKey faction,
                                           FactionGoal goal) noexcept {
    const auto key = input.world_seed ^
                     (static_cast<std::uint64_t>(faction) << 48U) ^
                     (static_cast<std::uint64_t>(goal) << 40U) ^
                     static_cast<std::uint64_t>(input.xun);
    return static_cast<std::int32_t>(mix64(key) % 121U) - 60;
}

[[nodiscard]] std::int32_t strongest_pressure(const FactionView& view) noexcept {
    std::int32_t strongest{};
    for (const auto& estimate : view.estimates()) {
        strongest = std::max(strongest, saturating_score(
            static_cast<std::int64_t>(estimate.military_power) + estimate.economic_power));
    }
    const auto own = std::max<std::int64_t>(
        1, static_cast<std::int64_t>(view.own_military_power()) +
               view.own_economic_power());
    return saturating_score(static_cast<std::int64_t>(strongest) * 100 / own);
}

[[nodiscard]] std::int32_t goal_score(const FactionView& view,
                                      const FactionPersonality& personality,
                                      const GoalSelectionInput& input,
                                      FactionGoal goal) noexcept {
    std::int64_t score{};
    const auto pressure = strongest_pressure(view);
    switch (goal) {
    case FactionGoal::Colonize:
        score = personality.expansion * 5 + personality.commerce;
        break;
    case FactionGoal::Conquer:
        score = personality.aggression * 5 + personality.resentment * 2 -
                personality.caution * 2;
        break;
    case FactionGoal::Prosper:
        score = personality.commerce * 6 + personality.caution;
        break;
    case FactionGoal::Ally:
        score = personality.fidelity * 5 + personality.caution * 2 + pressure;
        break;
    case FactionGoal::Survive:
        score = personality.caution * 6 + pressure * 2 - personality.aggression;
        break;
    case FactionGoal::HolyWar:
        score = personality.piety * 6 + personality.aggression * 2;
        break;
    }
    return saturating_score(score + situation_noise(input, view.observer(), goal));
}

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

FactionGoal update_faction_goal(const FactionView& view,
                                const FactionPersonality& personality,
                                const GoalSelectionInput& input,
                                FactionMindState& state) noexcept {
    if (state.forced_goal.has_value()) {
        state.goal = *state.forced_goal;
        state.goal_score = goal_score(view, personality, input, state.goal);
        state.initialized = true;
        return state.goal;
    }
    constexpr std::array goals{FactionGoal::Colonize, FactionGoal::Conquer,
                               FactionGoal::Prosper, FactionGoal::Ally,
                               FactionGoal::Survive, FactionGoal::HolyWar};
    auto best = goals.front();
    auto best_score = goal_score(view, personality, input, best);
    for (const auto goal : goals) {
        const auto score = goal_score(view, personality, input, goal);
        if (score > best_score || (score == best_score && goal < best)) {
            best = goal;
            best_score = score;
        }
    }
    if (!state.initialized) {
        state.goal = best;
        state.goal_score = best_score;
        state.initialized = true;
    } else {
        const auto current_score = goal_score(view, personality, input, state.goal);
        if (best != state.goal &&
            best_score > saturating_score(static_cast<std::int64_t>(current_score) +
                                          input.switch_threshold)) {
            state.goal = best;
            ++state.goal_switches;
        }
        state.goal_score = goal_score(view, personality, input, state.goal);
    }
    return state.goal;
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

PowerChange faction_power_change(FactionActionKind action, FactionAiLod lod,
                                 std::int32_t military_power,
                                 std::int32_t economic_power,
                                 std::uint64_t world_seed, std::int64_t xun,
                                 FactionKey faction) noexcept {
    const auto total = std::max<std::int64_t>(
        1, static_cast<std::int64_t>(military_power) + economic_power);
    // 三旬內的 -1/0/+1 恰好守恆；seed、勢力與 LOD 只旋轉相位，
    // 避免有限樣本因各級使用不同隨機 channel 而留下固定方向偏差。
    const auto phase = (static_cast<std::uint64_t>(xun) +
                        static_cast<std::uint64_t>(faction) +
                        static_cast<std::uint64_t>(lod) + world_seed % 3U) % 3U;
    const auto jitter = static_cast<std::int32_t>(phase) - 1;
    const auto growth = std::max<std::int32_t>(
        1, saturating_score(total / 200 + jitter));
    std::int32_t military_share = growth / 2;
    switch (action) {
    case FactionActionKind::Develop:
    case FactionActionKind::OfferAlliance:
    case FactionActionKind::PayTribute:
        military_share = growth / 4;
        break;
    case FactionActionKind::Prepare:
    case FactionActionKind::DeclareWar:
        military_share = growth * 3 / 4;
        break;
    case FactionActionKind::Expand:
    case FactionActionKind::StatisticalProgress:
        break;
    }
    return {military_share, growth - military_share};
}

} // namespace aetheria::ai

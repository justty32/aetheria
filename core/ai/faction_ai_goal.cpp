// 勢力 AI 的目標評分與切換慣性。

#include <aetheria/ai/faction_ai.h>

#include "faction_ai_detail.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace aetheria::ai {
using detail::saturating_score;

namespace {

[[nodiscard]] constexpr std::uint64_t mix64(std::uint64_t value) noexcept {
    value += UINT64_C(0x9e3779b97f4a7c15);
    value = (value ^ (value >> 30U)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31U);
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

} // namespace

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

} // namespace aetheria::ai

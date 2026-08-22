#pragma once

// 勢力 AI 的受限決策介面：只讀 FactionView，以目標慣性與整數效用選出玩家同型命令。
// 戰鬥預測由呼叫端注入；實作 target 看不到 WorldDiplomacyState。

#include <aetheria/ai/faction_view.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace aetheria::ai {

enum class FactionGoal : std::uint8_t {
    Colonize,
    Conquer,
    Prosper,
    Ally,
    Survive,
    HolyWar,
};

enum class FactionActionKind : std::uint8_t {
    Develop,
    Prepare,
    Expand,
    DeclareWar,
    OfferAlliance,
    PayTribute,
    StatisticalProgress,
};

enum class FactionAiLod : std::uint8_t { Full, Simplified, Statistical };

struct FactionPersonality {
    std::int32_t expansion{};
    std::int32_t aggression{};
    std::int32_t fidelity{};
    std::int32_t commerce{};
    std::int32_t piety{};
    std::int32_t caution{};
    std::int32_t resentment{};

    constexpr bool operator==(const FactionPersonality&) const noexcept = default;
};

struct FactionMindState {
    FactionGoal goal{FactionGoal::Prosper};
    std::int32_t goal_score{};
    std::uint32_t goal_switches{};
    bool initialized{};
    std::optional<FactionGoal> forced_goal;

    constexpr bool operator==(const FactionMindState&) const noexcept = default;
};

struct GoalSelectionInput {
    std::uint64_t world_seed{};
    std::int64_t xun{};
    std::int32_t switch_threshold{};
};

enum class BattleAssessment : std::uint8_t { LikelyWin, LikelyLoss, Disengagement };

using BattleEvaluatorFunction = BattleAssessment (*)(std::int32_t, std::int32_t,
                                                       const void*) noexcept;

struct BattleEvaluator {
    BattleEvaluatorFunction function{};
    const void* context{};

    [[nodiscard]] BattleAssessment operator()(std::int32_t own_power,
                                              std::int32_t target_power) const noexcept;
};

struct FactionCommand {
    FactionActionKind kind{FactionActionKind::Develop};
    FactionKey issuer{};
    FactionKey target{};

    constexpr bool operator==(const FactionCommand&) const noexcept = default;
};

struct ScoredFactionAction {
    FactionCommand command;
    std::int32_t utility{};

    constexpr bool operator==(const ScoredFactionAction&) const noexcept = default;
};

struct FactionDecision {
    FactionGoal goal{FactionGoal::Prosper};
    FactionCommand command;
    std::vector<ScoredFactionAction> scored_actions;
};

struct PowerChange {
    std::int32_t military{};
    std::int32_t economic{};
};

[[nodiscard]] FactionGoal update_faction_goal(const FactionView& view,
                                              const FactionPersonality& personality,
                                              const GoalSelectionInput& input,
                                              FactionMindState& state) noexcept;

[[nodiscard]] ScoredFactionAction
select_highest_utility(std::span<const ScoredFactionAction> actions) noexcept;

[[nodiscard]] FactionDecision decide_full(const FactionView& view,
                                          const FactionPersonality& personality,
                                          const GoalSelectionInput& goal_input,
                                          FactionMindState& state,
                                          BattleEvaluator battle);

[[nodiscard]] FactionDecision decide_simplified(const FactionView& view,
                                                const FactionPersonality& personality,
                                                FactionMindState& state);

[[nodiscard]] FactionDecision decide_statistical(const FactionView& view,
                                                 FactionMindState& state) noexcept;

[[nodiscard]] PowerChange faction_power_change(FactionActionKind action, FactionAiLod lod,
                                               std::int32_t military_power,
                                               std::int32_t economic_power,
                                               std::uint64_t world_seed, std::int64_t xun,
                                               FactionKey faction) noexcept;

} // namespace aetheria::ai

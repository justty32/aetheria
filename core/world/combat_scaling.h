#pragma once

// combat_scaling.h：以 Region 戰鬥公式為權威期望，提供三層抽樣、事件升降格、
// significance 個人貢獻上界與單一主場寫入計數。

#include <array>
#include <cstdint>
#include <vector>

#include "core/rules/combat.h"
#include "core/world/significance.h"

namespace aetheria::world {

enum class CombatLayer : std::uint8_t { Region, Site, Local };

struct CombatScalingRules {
    // Region 最穩定；Site 用真實戰術壓力挪動傷亡，Local 仍增加零均值方差。
    std::array<std::int32_t, 3> random_spread_permyriad{200, 600, 1'200};
    // Ambient、Local、Site、Region、World：可偏離敵方初始戰力的比例。
    std::array<std::int32_t, 5> contribution_delta_permyriad{0, 100, 500, 2'000, 10'000};
    // 正常固定為 0；故障注入用 +300 證明符號守門能抓到系統性 +3%。
    std::int32_t site_systematic_bias_permyriad{};
};

struct CombatPhase {
    std::uint32_t index{};
    std::uint32_t count{1};
};

struct PersonalContribution {
    Significance significance{Significance::Ambient};
    // 正值增加 B 方損失，負值降低 B 方損失。
    std::int32_t requested_loss_shift_b{};
};

struct ContributionAllocation {
    std::int32_t statistical_loss{};
    std::int32_t delta_limit{};
    std::int32_t requested_deviation{};
    std::int32_t applied_deviation{};
    std::int32_t final_loss{};
    std::int32_t named_loss{};
    std::int32_t unnamed_loss{};

    constexpr bool operator==(const ContributionAllocation&) const noexcept = default;
};

struct CombatExecutionCounters {
    std::uint64_t region_face_runs{};
    std::uint64_t site_face_runs{};
    std::uint64_t local_face_runs{};
    std::uint64_t region_face_damage_writes{};
    std::uint64_t site_reduction_writes{};
};

struct LayerCombatResult {
    CombatLayer layer{CombatLayer::Region};
    std::uint64_t resolution_id{};
    std::int32_t region_expected_loss_a{};
    std::int32_t region_expected_loss_b{};
    std::int32_t loss_a{};
    std::int32_t loss_b{};
    ContributionAllocation contribution_b{};
};

struct NamedCombatantState {
    std::uint64_t participant_id{};
    Significance significance{Significance::Ambient};
    std::int32_t wounds{};

    constexpr bool operator==(const NamedCombatantState&) const noexcept = default;
};

struct CombatEventState {
    std::uint64_t event_id{};
    std::int32_t initial_power_a{};
    std::int32_t initial_power_b{};
    std::int32_t accumulated_loss_a{};
    std::int32_t accumulated_loss_b{};
    std::uint64_t last_resolution_id{};
    std::vector<NamedCombatantState> named;

    bool operator==(const CombatEventState&) const noexcept = default;
};

// 兩個 uint64_t 沒有 padding；測試可把整個 placement 序列逐位元比較。
struct PackedCombatPlacement {
    std::uint64_t unit_id{};
    std::uint64_t packed_position{};

    constexpr bool operator==(const PackedCombatPlacement&) const noexcept = default;
};

struct CombatPromotion {
    std::vector<PackedCombatPlacement> placements;
    std::vector<NamedCombatantState> named;

    bool operator==(const CombatPromotion&) const noexcept = default;
};

[[nodiscard]] std::int32_t contribution_delta_limit(
    Significance significance, std::int32_t opposing_power,
    const CombatScalingRules& scaling = {}) noexcept;

[[nodiscard]] ContributionAllocation apply_personal_contribution(
    std::int32_t statistical_loss, std::int32_t opposing_power,
    PersonalContribution contribution, const CombatScalingRules& scaling = {}) noexcept;

[[nodiscard]] LayerCombatResult resolve_scaled_combat(
    const rules::CombatInput& input, const rules::CombatRules& combat_rules,
    CombatLayer layer, std::uint64_t event_id, std::uint64_t sample_seed,
    CombatPhase phase = {}, PersonalContribution contribution = {},
    CombatExecutionCounters* counters = nullptr,
    const CombatScalingRules& scaling = {}) noexcept;

[[nodiscard]] CombatPromotion promote_combat_event(const CombatEventState& state,
                                                    std::uint64_t target_zone_id,
                                                    std::int64_t promote_tick);

// 同一 resolution_id 只會歸約一次；程序層升降本身不改統計或具名狀態。
[[nodiscard]] bool demote_combat_event(CombatEventState& state,
                                       const LayerCombatResult& result,
                                       CombatExecutionCounters* counters = nullptr) noexcept;

}  // namespace aetheria::world

#pragma once

// core/rules/power.h：資料驅動的力量位階、等效戰力與個體階差門檻。
// 戰鬥位階直接使用 world::Significance，不另立等級列舉。

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "core/world/significance.h"

namespace aetheria::rules {

inline constexpr std::size_t kPowerTierCount =
    static_cast<std::size_t>(world::Significance::World) + 1U;

// PowerBreakthroughDefId 是 power.toml 破階手段 def 的執行期下標。
// 它只辨識資料 def，不是破階手段種類的封閉列舉。
enum class PowerBreakthroughDefId : std::uint16_t {};

[[nodiscard]] constexpr std::uint16_t value_of(PowerBreakthroughDefId id) noexcept {
    return static_cast<std::uint16_t>(id);
}

struct PowerBreakthroughDef {
    std::string id;
    std::string name_key;
};

// PowerRules 的數值全部由 power.toml 載入。等效戰力保留百分點比例，
// 因此 reference_quality_percent 下的一點權重會得到 reference_quality_percent 分。
struct PowerRules {
    std::array<std::int64_t, kPowerTierCount> tier_weights{};
    std::int32_t minimum_quality_percent{};
    std::int32_t reference_quality_percent{};
    std::int32_t maximum_quality_percent{};
    std::uint8_t individual_gate_minimum_gap{};
    std::uint16_t gated_damage_numerator{};
    std::uint16_t gated_damage_denominator{};
    bool loaded{};
};

// PowerStack 是共享 tier 與 quality 的同質單位集合；count 是量，tier 是質。
struct PowerStack {
    std::int64_t count{};
    world::Significance tier{world::Significance::Ambient};
    std::int32_t quality_percent{};
};

// 回傳百分點定點值的 S；輸入違反資料值域或 int64 乘加溢位時 AETH_CHECK。
[[nodiscard]] std::int64_t equivalent_power(std::span<const PowerStack> stacks,
                                             const PowerRules& rules) noexcept;

// 人數只增加 S；cohort tier 是非空 stack 的最高戰鬥位階，空 cohort 為 Ambient。
[[nodiscard]] world::Significance cohort_tier(std::span<const PowerStack> stacks) noexcept;

// 門檻只作用於分配給個體的傷害。breakthrough 非空表示攻擊帶有 Ruleset def；
// cohort 傷害或有效破階 def 均原樣回傳 proposed_damage。
[[nodiscard]] std::int64_t apply_individual_tier_gate(
    std::int64_t proposed_damage, world::Significance attacker_tier,
    world::Significance defender_tier, bool defender_is_individual, const PowerRules& rules,
    const PowerBreakthroughDef* breakthrough = nullptr) noexcept;

}  // namespace aetheria::rules

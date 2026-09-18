#pragma once

// Region 戰役結算跨檔共用的單方損失結果與計算宣告。

#include "core/rules/combat.h"

namespace aetheria::rules::detail {

struct SideLoss {
    LossBreakdown breakdown{};
    std::int32_t collapse_threshold{};
    bool routed{};
};

[[nodiscard]] SideLoss calculate_side_loss(const CombatSideInput& side, FixedPowerFactor factor,
                                            std::int32_t duration,
                                            const CombatRules& rules) noexcept;

[[nodiscard]] std::int32_t morale_delta(const CombatSideInput& side, const SideLoss& loss,
                                        const CombatRules& rules) noexcept;

}  // namespace aetheria::rules::detail

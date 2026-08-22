#pragma once

// 和談籌碼的公開整數純函式；玩家與 AI 共用這一份公式。
// 本標頭不依賴世界真值型別，可安全暴露給受限的 AI target。

#include <algorithm>
#include <cstdint>
#include <limits>

namespace aetheria::diplomacy {

struct PeaceLeverageInput {
    std::int32_t war_score{};
    std::int32_t own_weariness{};
    std::int32_t opponent_weariness{};
    std::int32_t third_party_pressure{};

    constexpr bool
    operator==(const PeaceLeverageInput&) const noexcept = default;
};

struct PeaceLeverageWeights {
    std::int32_t war_score{};
    std::int32_t own_weariness{};
    std::int32_t opponent_weariness{};
    std::int32_t third_party_pressure{};
    std::int32_t divisor{1};

    constexpr bool
    operator==(const PeaceLeverageWeights&) const noexcept = default;
};

struct PeaceTermThresholds {
    std::int32_t reparations{};
    std::int32_t cede_territory{};
    std::int32_t vassalage{};

    constexpr bool
    operator==(const PeaceTermThresholds&) const noexcept = default;
};

enum class PeaceTerms : std::uint8_t {
    Ceasefire,
    Reparations,
    CedeTerritory,
    Vassalage,
};

[[nodiscard]] constexpr std::int32_t
calculate_peace_leverage(PeaceLeverageInput input,
                         PeaceLeverageWeights weights) noexcept {
    const auto weighted =
        static_cast<std::int64_t>(input.war_score) * weights.war_score -
        static_cast<std::int64_t>(input.own_weariness) * weights.own_weariness +
        static_cast<std::int64_t>(input.opponent_weariness) *
            weights.opponent_weariness +
        static_cast<std::int64_t>(input.third_party_pressure) *
            weights.third_party_pressure;
    const auto divisor = std::max<std::int32_t>(1, weights.divisor);
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(
        weighted / divisor, std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::max()));
}

[[nodiscard]] constexpr PeaceTerms
derive_peace_terms(std::int32_t leverage,
                   PeaceTermThresholds thresholds) noexcept {
    if (leverage >= thresholds.vassalage) {
        return PeaceTerms::Vassalage;
    }
    if (leverage >= thresholds.cede_territory) {
        return PeaceTerms::CedeTerritory;
    }
    if (leverage >= thresholds.reparations) {
        return PeaceTerms::Reparations;
    }
    return PeaceTerms::Ceasefire;
}

} // namespace aetheria::diplomacy

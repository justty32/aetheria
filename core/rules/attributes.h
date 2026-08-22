#pragma once

// core/rules/attributes.h：定義個體四屬性、資料驅動衍生值公式，以及
// 屬性到既有重要性位階的校準介面。

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "core/world/significance.h"

namespace aetheria::rules {

struct Attributes {
    std::int32_t body{};
    std::int32_t skill{};
    std::int32_t mind{};
    std::int32_t spirit{};

    constexpr bool operator==(const Attributes&) const noexcept = default;
};

struct DerivedStatFormula {
    std::int32_t base{};
    std::int32_t body{};
    std::int32_t skill{};
    std::int32_t mind{};
    std::int32_t spirit{};
    std::int32_t tier{};
    std::int32_t divisor{1};
};

struct DerivedStats {
    std::int32_t health{};
    std::int32_t mana{};
    std::int32_t accuracy{};
    std::int32_t evasion{};
    std::int32_t defense{};
    std::int32_t resistance{};
    std::int32_t movement{};
    std::int32_t carry_capacity{};
    std::int32_t vision{};

    constexpr bool operator==(const DerivedStats&) const noexcept = default;
};

using DerivedStatModifiers = DerivedStats;

struct AttributeRules {
    std::int32_t minimum{};
    std::int32_t maximum{};
    std::array<std::int32_t, 4> tier_thresholds{};
    DerivedStatFormula health;
    DerivedStatFormula mana;
    DerivedStatFormula accuracy;
    DerivedStatFormula evasion;
    DerivedStatFormula defense;
    DerivedStatFormula resistance;
    DerivedStatFormula movement;
    DerivedStatFormula carry_capacity;
    DerivedStatFormula vision;
};

struct TierOverride {
    world::Significance tier{};
    std::string reason;
};

[[nodiscard]] world::Significance suggest_tier(const Attributes& attributes,
                                               const AttributeRules& rules);
[[nodiscard]] world::Significance resolve_tier(
    const Attributes& attributes, const AttributeRules& rules,
    const std::optional<TierOverride>& override = std::nullopt);
[[nodiscard]] DerivedStats derive_stats(const Attributes& attributes, world::Significance tier,
                                        const DerivedStatModifiers& equipment,
                                        const AttributeRules& rules);

}  // namespace aetheria::rules

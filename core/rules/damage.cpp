// core/rules/damage.cpp：以稀疏型別抗性與整數算術結算傷害。

#include "core/rules/damage.h"

#include <algorithm>
#include <stdexcept>

namespace aetheria::rules {

DamageResult apply_resistance(std::int32_t base_damage, DamageTypeId type,
                              std::span<const DamageResistance> resistances,
                              const DamageRules& rules) {
    if (base_damage < 0) {
        throw std::invalid_argument{"基礎傷害不可為負"};
    }
    std::int32_t requested_resistance{};
    bool found{};
    for (const auto& resistance : resistances) {
        if (resistance.type != type) {
            continue;
        }
        if (found) {
            throw std::invalid_argument{"同一傷害型別的稀疏抗性重複"};
        }
        requested_resistance = resistance.percent;
        found = true;
    }
    const auto maximum = std::clamp(rules.max_resistance_percent, 0, 99);
    const auto applied_resistance = std::min(requested_resistance, maximum);
    const auto numerator =
        static_cast<std::int64_t>(base_damage) * (INT64_C(100) - applied_resistance);
    const auto actual_damage = base_damage == 0 ? 0 : std::max(INT64_C(1), numerator / 100);
    return {
        .base_damage = base_damage,
        .requested_resistance_percent = requested_resistance,
        .applied_resistance_percent = applied_resistance,
        .actual_damage = actual_damage,
    };
}

}  // namespace aetheria::rules

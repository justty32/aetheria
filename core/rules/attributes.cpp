// core/rules/attributes.cpp：以純整數公式計算衍生值，並校準個體建議位階。

#include "core/rules/attributes.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace aetheria::rules {
namespace {

void validate_attributes(const Attributes& attributes, const AttributeRules& rules) {
    const std::array values{attributes.body, attributes.skill, attributes.mind, attributes.spirit};
    if (std::ranges::any_of(values, [&](const auto value) {
            return value < rules.minimum || value > rules.maximum;
        })) {
        throw std::out_of_range{"個體屬性超出 AttributeRules 值域"};
    }
}

[[nodiscard]] std::int32_t calculate(const Attributes& attributes, world::Significance tier,
                                     const DerivedStatFormula& formula, std::int32_t equipment) {
    const auto tier_value = static_cast<std::int64_t>(tier);
    const auto numerator = static_cast<std::int64_t>(attributes.body) * formula.body +
                           static_cast<std::int64_t>(attributes.skill) * formula.skill +
                           static_cast<std::int64_t>(attributes.mind) * formula.mind +
                           static_cast<std::int64_t>(attributes.spirit) * formula.spirit +
                           tier_value * formula.tier;
    const auto value =
        static_cast<std::int64_t>(formula.base) + numerator / formula.divisor + equipment;
    if (value < std::numeric_limits<std::int32_t>::min() ||
        value > std::numeric_limits<std::int32_t>::max()) {
        throw std::overflow_error{"個體衍生值超出 int32"};
    }
    return static_cast<std::int32_t>(value);
}

}  // namespace

world::Significance suggest_tier(const Attributes& attributes, const AttributeRules& rules) {
    validate_attributes(attributes, rules);
    const auto total = static_cast<std::int64_t>(attributes.body) + attributes.skill +
                       attributes.mind + attributes.spirit;
    const auto average = total / 4;
    std::size_t tier_index{};
    for (const auto threshold : rules.tier_thresholds) {
        tier_index += average >= threshold ? 1U : 0U;
    }
    return static_cast<world::Significance>(tier_index);
}

world::Significance resolve_tier(const Attributes& attributes, const AttributeRules& rules,
                                 const std::optional<TierOverride>& override) {
    const auto suggested = suggest_tier(attributes, rules);
    if (!override.has_value()) {
        return suggested;
    }
    if (override->reason.empty()) {
        throw std::invalid_argument{"位階覆寫必須附理由"};
    }
    return override->tier;
}

DerivedStats derive_stats(const Attributes& attributes, world::Significance tier,
                          const DerivedStatModifiers& equipment, const AttributeRules& rules) {
    validate_attributes(attributes, rules);
    return {
        .health = calculate(attributes, tier, rules.health, equipment.health),
        .mana = calculate(attributes, tier, rules.mana, equipment.mana),
        .accuracy = calculate(attributes, tier, rules.accuracy, equipment.accuracy),
        .evasion = calculate(attributes, tier, rules.evasion, equipment.evasion),
        .defense = calculate(attributes, tier, rules.defense, equipment.defense),
        .resistance = calculate(attributes, tier, rules.resistance, equipment.resistance),
        .movement = calculate(attributes, tier, rules.movement, equipment.movement),
        .carry_capacity =
            calculate(attributes, tier, rules.carry_capacity, equipment.carry_capacity),
        .vision = calculate(attributes, tier, rules.vision, equipment.vision),
    };
}

}  // namespace aetheria::rules

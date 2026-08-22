// core/rules/check.cpp：實作單骰 d100 判定與餘量分段，不另擲暴擊骰。

#include "core/rules/check.h"

#include <limits>
#include <stdexcept>

namespace aetheria::rules {

CheckResult evaluate_check(std::uint8_t roll, std::int32_t attribute, std::int32_t modifier,
                           std::int32_t difficulty, const CheckRules& rules) {
    if (roll > 99U) {
        throw std::out_of_range{"d100 擲骰值必須介於 0 與 99"};
    }
    if (rules.margin_bands.empty() ||
        rules.margin_bands.size() > std::numeric_limits<std::uint16_t>::max()) {
        throw std::invalid_argument{"d100 餘量分段表為空或過大"};
    }
    const auto target = static_cast<std::int64_t>(attribute) + modifier - difficulty;
    const auto margin = target - roll;
    std::size_t band_index{};
    for (std::size_t index = 1; index < rules.margin_bands.size(); ++index) {
        if (margin < rules.margin_bands[index].minimum_margin) {
            break;
        }
        band_index = index;
    }
    return {
        .roll = roll,
        .target = target,
        .margin = margin,
        .success = roll < target,
        .band_index = static_cast<std::uint16_t>(band_index),
        .effect_percent = rules.margin_bands[band_index].effect_percent,
    };
}

CheckResult perform_check(std::mt19937_64& rng, std::int32_t attribute, std::int32_t modifier,
                          std::int32_t difficulty, const CheckRules& rules) {
    const auto roll = static_cast<std::uint8_t>(rng() % UINT64_C(100));
    return evaluate_check(roll, attribute, modifier, difficulty, rules);
}

}  // namespace aetheria::rules

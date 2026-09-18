#pragma once

#include "core/world/combat_scaling.h"
#include "tests/support/performance.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <span>
#include <vector>

#include <gtest/gtest.h>

namespace aetheria::tests::combat_scaling {

using aetheria::rules::CombatInput;
using aetheria::rules::CombatModifiers;
using aetheria::world::CombatEventState;
using aetheria::world::CombatExecutionCounters;
using aetheria::world::CombatLayer;
using aetheria::world::CombatPhase;
using aetheria::world::CombatScalingRules;
using aetheria::world::NamedCombatantState;
using aetheria::world::PersonalContribution;
using aetheria::world::Significance;

inline constexpr std::size_t kCalibrationSamples = 1'000;

inline constexpr std::uint64_t kEventBase = UINT64_C(0x6d37000000000000);

[[nodiscard]] inline CombatModifiers neutral(const aetheria::rules::CombatRules& rules) {
    return {rules.modifier_scale, rules.modifier_scale, rules.modifier_scale,
            rules.modifier_scale, rules.modifier_scale};
}

[[nodiscard]] inline CombatInput matchup(std::int32_t power_a, std::int32_t power_b) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    return {{power_a, neutral(rules), {}, 0},
            {power_b, neutral(rules), {}, 0},
            rules.default_exponent,
            1};
}

[[nodiscard]] inline double sample_variance(std::span<const double> samples) {
    const auto mean = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
    double squared{};
    for (const auto sample : samples) {
        const auto difference = sample - mean;
        squared += difference * difference;
    }
    return squared / static_cast<double>(samples.size() - 1U);
}

}  // namespace aetheria::tests::combat_scaling

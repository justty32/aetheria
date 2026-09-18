#pragma once

#include "core/rules/combat.h"
#include "core/rules/ruleset.h"
#include "tests/support/ruleset_fixture.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <numeric>

#include <gtest/gtest.h>

namespace aetheria::tests::combat_support {

using namespace aetheria::rules;
using aetheria::world::Significance;

[[nodiscard]] inline CombatModifiers neutral(const CombatRules& rules) {
    return {rules.modifier_scale, rules.modifier_scale, rules.modifier_scale,
            rules.modifier_scale, rules.modifier_scale};
}

[[nodiscard]] inline CombatInput matchup(std::int32_t power_a, std::int32_t power_b,
                                  const CombatRules& rules) {
    return {{power_a, neutral(rules), {}, 0},
            {power_b, neutral(rules), {}, 0},
            rules.default_exponent,
            1};
}

} // namespace aetheria::tests::combat_support

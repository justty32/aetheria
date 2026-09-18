#pragma once

#include "core/rules/combat.h"
#include "core/rules/power.h"
#include "core/rules/power_sources.h"
#include "core/rules/ruleset.h"
#include "tests/rules/ruleset_test_support.h"
#include "tests/support/ruleset_fixture.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace aetheria::tests::power_sources_support {

using namespace aetheria::rules;
using aetheria::world::Significance;

[[nodiscard]] inline PowerSourceDef source(PowerSourceKind kind, std::string id,
                                    std::int8_t bonus, std::string ability,
                                    Attributes modifier = {}) {
    std::vector<std::string> abilities;
    if (!ability.empty()) {
        abilities.push_back(std::move(ability));
    }
    return {
        .kind = kind,
        .origin_id = std::move(id),
        .tier_bonus = bonus,
        .ability_ids = std::move(abilities),
        .attribute_modifier = modifier,
        .removal_condition = kind == PowerSourceKind::Bloodline
                                 ? "condition.bloodline_lost"
                                 : "condition.source_deprived",
    };
}

} // namespace aetheria::tests::power_sources_support

#pragma once

#include "core/site/site_combat.h"
#include "tests/support/ruleset_fixture.h"

namespace aetheria::tests::site_combat {

[[nodiscard]] inline aetheria::rules::CombatModifiers neutral() {
    const auto scale = aetheria::tests::test_ruleset().combat_rules().modifier_scale;
    return {scale, scale, scale, scale, scale};
}

}  // namespace aetheria::tests::site_combat

#pragma once

#include "core/rules/combat.h"
#include "core/world/faction_ai.h"
#include "tests/support/performance.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

namespace aetheria::tests::faction_ai {

using namespace aetheria;
using tests::test_ruleset;
using world::FactionId;
using world::WorldDiplomacyState;

[[nodiscard]] inline WorldDiplomacyState observed_world(std::uint64_t seed = 7) {
    WorldDiplomacyState world{3, seed, test_ruleset()};
    world.set_faction_truth(FactionId{1}, 1200, 900);
    world.set_faction_truth(FactionId{2}, 1200, 900);
    world.set_faction_truth(FactionId{3}, 1200, 900);
    for (std::uint16_t observer = 1; observer <= 3; ++observer) {
        for (std::uint16_t target = 1; target <= 3; ++target) {
            if (observer != target) {
                world.observe_faction(FactionId{observer}, FactionId{target}, 0,
                                      time::Tick{0}, 3);
            }
        }
    }
    return world;
}

}  // namespace aetheria::tests::faction_ai

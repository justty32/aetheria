#pragma once

#include "core/local/local_combat.h"
#include "core/site/site_combat.h"
#include "tests/local/local_navigation_test_support.h"
#include "tests/support/performance.h"

#include <aetheria/runtime/cross_zone.h>

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

namespace aetheria::tests::local_combat_support {

using aetheria::local::LayerCombatState;
using aetheria::local::LocalCombatant;
using aetheria::local::LocalCombatRoundInput;
using aetheria::local::LocalCombatSide;
using aetheria::local::LocalCombatState;
using aetheria::local::LocalCombatStats;
using aetheria::local::SiteCombatBoundaryCondition;
using aetheria::spatial::BoundarySide;
using aetheria::tests::kNavigationCenter;
using aetheria::tests::navigation_zone;
using aetheria::tests::set_navigation_edge;
using aetheria::tests::test_ruleset;
using aetheria::world::Significance;
using aetheria::zone::InMemoryZoneStore;
using aetheria::zone::ZoneManager;

inline constexpr SiteCombatBoundaryCondition kWestEastBoundary{
    BoundarySide::West, BoundarySide::East, BoundarySide::West,
    BoundarySide::East};

[[nodiscard]] inline LocalCombatant
unit(std::uint64_t id, LocalCombatSide side, aetheria::local::LocalXY tile,
     Significance significance = Significance::Ambient,
     LocalCombatStats stats = {}) {
  return {.unit_id = id,
          .side = side,
          .location = {kNavigationCenter, tile},
          .significance = significance,
          .stats = stats};
}

[[nodiscard]] inline LocalCombatRoundInput
round_input(std::int32_t loss_a = 10'000, std::int32_t loss_b = 10'000,
            std::int32_t power_a = 100'000, std::int32_t power_b = 100'000) {
  return {.site_expected_loss = {loss_a, loss_b},
          .side_power = {power_a, power_b},
          .retreat_requested = {},
          .pursue_retreat = {},
          .personal_actions = {}};
}

} // namespace aetheria::tests::local_combat_support

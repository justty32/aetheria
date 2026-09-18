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

#include "tests/local/local_combat_test_support.h"

namespace {

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

using namespace aetheria::tests::local_combat_support;

TEST(LocalCombat, SiteBoundaryDeploysEnemyOnTheExactSuppliedEastEdge) {
  LocalCombatState state;
  state.combatants = {unit(1, LocalCombatSide::A, {}),
                      unit(2, LocalCombatSide::B, {}),
                      unit(3, LocalCombatSide::B, {})};

  aetheria::local::deploy_local_combatants(state, kNavigationCenter,
                                           kWestEastBoundary);

  EXPECT_EQ(state.combatants[0].location.tile.x, 0);
  EXPECT_EQ(state.combatants[1].location.tile.x, 63);
  EXPECT_EQ(state.combatants[2].location.tile.x, 63);
  std::cout << "local_boundary_authority side_B_entry=East x="
            << state.combatants[1].location.tile.x << ','
            << state.combatants[2].location.tile.x << '\n';
}

TEST(LocalCombat, SixSecondStrideMovesFiveTilesThroughM5FovPathAndMovement) {
  auto zone = navigation_zone(kNavigationCenter);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  aetheria::runtime::CrossZoneRuntime runtime{manager};
  LocalCombatState state;
  state.combatants = {unit(1, LocalCombatSide::A, {10, 10}),
                      unit(2, LocalCombatSide::B, {17, 10})};

  const auto result = aetheria::local::resolve_local_combat_round(
      state, runtime, test_ruleset(), kWestEastBoundary, round_input(), 20);

  static_assert(aetheria::local::kCombatStride == aetheria::time::Duration{6});
  static_assert(aetheria::local::kCombatMovementTiles == 5U);
  EXPECT_EQ(result.moved_tiles[0], 5U);
  EXPECT_EQ(state.combatants[0].location.tile,
            (aetheria::local::LocalXY{15, 10}));
  EXPECT_GT(result.attacks[0] + result.attacks[1], 0U);
  std::cout << "local_stride_seconds=6 movement_tiles_A="
            << result.moved_tiles[0]
            << " attacks=" << result.attacks[0] + result.attacks[1] << '\n';
}

TEST(LocalCombat, M5WallOcclusionMakesTheLocalCombatAttackTestRedIfBypassed) {
  auto zone = navigation_zone(kNavigationCenter);
  const auto wall = *test_ruleset().find_edge("edge.house_wall");
  set_navigation_edge(*zone, {32, 32}, BoundarySide::East, wall);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  aetheria::runtime::CrossZoneRuntime runtime{manager};
  LocalCombatState state;
  state.combatants = {unit(1, LocalCombatSide::A, {32, 32}),
                      unit(2, LocalCombatSide::B, {33, 32})};

  const auto result = aetheria::local::resolve_local_combat_round(
      state, runtime, test_ruleset(), kWestEastBoundary, round_input(), 22);

  EXPECT_EQ(result.attacks, (std::array<std::size_t, 2>{0, 0}));
  EXPECT_EQ(result.moved_tiles, (std::array<std::size_t, 2>{0, 0}));
  std::cout << "local_m5_dependency wall_occluded_attacks="
            << result.attacks[0] + result.attacks[1] << '\n';
}

TEST(LocalCombat, SameSeedReplaysEveryUnitRollMoveAndAggregate) {
  auto zone = navigation_zone(kNavigationCenter);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  aetheria::runtime::CrossZoneRuntime runtime{manager};
  LocalCombatState first;
  first.combatants = {unit(1, LocalCombatSide::A, {30, 30}),
                      unit(2, LocalCombatSide::B, {31, 30})};
  auto second = first;

  const auto first_result = aetheria::local::resolve_local_combat_round(
      first, runtime, test_ruleset(), kWestEastBoundary, round_input(), 0xD100);
  const auto second_result = aetheria::local::resolve_local_combat_round(
      second, runtime, test_ruleset(), kWestEastBoundary, round_input(),
      0xD100);

  EXPECT_EQ(first_result, second_result);
  EXPECT_EQ(first, second);
  EXPECT_EQ(first_result.attacks, (std::array<std::size_t, 2>{1, 1}));
}

TEST(LocalCombat, PerRoundUsesWarmMinimumOfFiveUnderTwoMilliseconds) {
  auto zone = navigation_zone(kNavigationCenter);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  aetheria::runtime::CrossZoneRuntime runtime{manager};
  aetheria::local::LocalCombatRoundResult measured;
  const auto measure = [&] {
    LocalCombatState state;
    const LocalCombatStats symmetric{70, 20, 10, 2};
    for (std::uint16_t offset = 0; offset < 8; ++offset) {
      state.combatants.push_back(
          unit(1U + offset, LocalCombatSide::A,
               {31, static_cast<std::uint16_t>(24U + offset)},
               Significance::Ambient, symmetric));
      state.combatants.push_back(
          unit(101U + offset, LocalCombatSide::B,
               {32, static_cast<std::uint16_t>(24U + offset)},
               Significance::Ambient, symmetric));
    }
    const auto start = std::chrono::steady_clock::now();
    measured = aetheria::local::resolve_local_combat_round(
        state, runtime, test_ruleset(), kWestEastBoundary, round_input(), 32);
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - start)
        .count();
  };
  const auto milliseconds =
      aetheria::tests::minimum_milliseconds_after_warmup(measure);

  EXPECT_EQ(measured.attacks, (std::array<std::size_t, 2>{8, 8}));
  EXPECT_LT(milliseconds, 2.0);
  std::cout << "local_combat_per_round units=16 attacks=16 warmup=1 measured=5"
            << " minimum_ms=" << milliseconds << " budget_ms=2\n";
}

} // namespace

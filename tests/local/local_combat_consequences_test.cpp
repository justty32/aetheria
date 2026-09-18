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

TEST(LocalCombat,
     SameStatsDifferentSignificanceProduceTwoDifferentInfluencesInOneBattle) {
  auto zone = navigation_zone(kNavigationCenter);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  aetheria::runtime::CrossZoneRuntime runtime{manager};
  const LocalCombatStats identical{70, 20, 10, 0};
  LocalCombatState state;
  state.combatants = {
      unit(10, LocalCombatSide::A, {10, 10}, Significance::Ambient, identical),
      unit(11, LocalCombatSide::A, {11, 10}, Significance::World, identical),
      unit(20, LocalCombatSide::B, {50, 50}, Significance::Ambient, identical)};
  auto input = round_input(12'000, 12'000);
  input.personal_actions = {{10, 1'000'000}, {11, 1'000'000}};

  const auto result = aetheria::local::resolve_local_combat_round(
      state, runtime, test_ruleset(), kWestEastBoundary, input, 24);

  ASSERT_EQ(result.contributions.size(), 2U);
  EXPECT_EQ(result.contributions[0].allocation.applied_deviation, 0);
  EXPECT_EQ(result.contributions[1].allocation.applied_deviation, 88'000);
  EXPECT_EQ(result.loss[1], 100'000);
  EXPECT_EQ(state.combatants[0].stats, state.combatants[1].stats);
  EXPECT_NE(state.combatants[0].significance, state.combatants[1].significance);
  std::cout << "local_delta_same_stats ambient_impact="
            << result.contributions[0].allocation.applied_deviation
            << " world_impact="
            << result.contributions[1].allocation.applied_deviation
            << " final_loss_B=" << result.loss[1] << '\n';
}

TEST(LocalCombat,
     SuccessfulLocalRetreatRemainsRetreatAtSiteAndRegionWithPursuitLoss) {
  auto zone = navigation_zone(kNavigationCenter);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  aetheria::runtime::CrossZoneRuntime runtime{manager};
  LocalCombatState state;
  state.combatants = {unit(1, LocalCombatSide::A, {0, 20}),
                      unit(2, LocalCombatSide::B, {1, 20})};
  state.combatants[0].health = 100;
  auto input = round_input();
  input.retreat_requested[0] = true;
  input.pursue_retreat[1] = true;

  const auto result = aetheria::local::resolve_local_combat_round(
      state, runtime, test_ruleset(), kWestEastBoundary, input, 26);

  EXPECT_EQ(result.layers[0].local, LayerCombatState::Retreated);
  EXPECT_EQ(result.layers[0].site, LayerCombatState::Retreated);
  EXPECT_EQ(result.layers[0].region, LayerCombatState::Retreated);
  EXPECT_EQ(result.layers[0].region_retreat_direction, BoundarySide::West);
  EXPECT_NE(result.layers[0].region, LayerCombatState::Annihilated);
  EXPECT_EQ(result.layers[1].region, LayerCombatState::Pursuing);
  EXPECT_GT(result.pursuit_loss[0], 0);
  std::cout << "local_retreat local=Retreated site=Retreated region=Retreated"
            << " region_exit=West pursuit_loss=" << result.pursuit_loss[0]
            << '\n';
}

TEST(LocalCombat, KillingCommanderMovesOneSharedMoraleAcrossAllThreeLayers) {
  auto zone = navigation_zone(kNavigationCenter);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  aetheria::runtime::CrossZoneRuntime runtime{manager};
  const LocalCombatStats certain_hit{100, 0, 10, 2};
  LocalCombatState state;
  state.combatants = {
      unit(1, LocalCombatSide::A, {32, 32}, Significance::Ambient, certain_hit),
      unit(2, LocalCombatSide::B, {33, 32}, Significance::Region, certain_hit)};
  state.combatants[1].commander = true;
  state.combatants[1].health = 1;
  const auto before = state.morale[1];

  static_cast<void>(aetheria::local::resolve_local_combat_round(
      state, runtime, test_ruleset(), kWestEastBoundary, round_input(), 28));

  EXPECT_FALSE(state.combatants[1].alive);
  EXPECT_EQ(before, (aetheria::local::SharedMorale{80, 80, 80}));
  EXPECT_EQ(state.morale[1], (aetheria::local::SharedMorale{55, 55, 55}));
  std::cout << "local_commander_morale before_L_S_R=" << before.local << '/'
            << before.site << '/' << before.region
            << " after_L_S_R=" << state.morale[1].local << '/'
            << state.morale[1].site << '/' << state.morale[1].region << '\n';
}

TEST(LocalCombat, RoutCreatesAggregateRemnantWithoutChangingCombatTier) {
  auto zone = navigation_zone(kNavigationCenter);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  aetheria::runtime::CrossZoneRuntime runtime{manager};
  LocalCombatState state;
  state.combatants = {unit(1, LocalCombatSide::A, {10, 10}),
                      unit(2, LocalCombatSide::A, {11, 10}),
                      unit(3, LocalCombatSide::B, {50, 50})};
  state.morale[0] = {20, 20, 20};
  aetheria::local::apply_local_morale_event(
      state, LocalCombatSide::A,
      aetheria::local::LocalMoraleEvent::AlliesRouted);

  const auto result = aetheria::local::resolve_local_combat_round(
      state, runtime, test_ruleset(), kWestEastBoundary, round_input(), 30);

  EXPECT_EQ(result.layers[0].local, LayerCombatState::Routed);
  EXPECT_EQ(result.layers[0].site, LayerCombatState::Routed);
  EXPECT_EQ(result.layers[0].region, LayerCombatState::Routed);
  ASSERT_TRUE(result.layers[0].routed_remnant.has_value());
  EXPECT_EQ(result.layers[0].routed_remnant->headcount, 2);
  EXPECT_EQ(result.layers[0].routed_remnant->aggregation_significance,
            Significance::Region);
}

} // namespace

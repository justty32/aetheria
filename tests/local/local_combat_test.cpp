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

constexpr SiteCombatBoundaryCondition kWestEastBoundary{
    BoundarySide::West, BoundarySide::East, BoundarySide::West,
    BoundarySide::East};

[[nodiscard]] LocalCombatant
unit(std::uint64_t id, LocalCombatSide side, aetheria::local::LocalXY tile,
     Significance significance = Significance::Ambient,
     LocalCombatStats stats = {}) {
  return {.unit_id = id,
          .side = side,
          .location = {kNavigationCenter, tile},
          .significance = significance,
          .stats = stats};
}

[[nodiscard]] LocalCombatRoundInput
round_input(std::int32_t loss_a = 10'000, std::int32_t loss_b = 10'000,
            std::int32_t power_a = 100'000, std::int32_t power_b = 100'000) {
  return {.site_expected_loss = {loss_a, loss_b},
          .side_power = {power_a, power_b},
          .retreat_requested = {},
          .pursue_retreat = {},
          .personal_actions = {}};
}

[[nodiscard]] aetheria::rules::CombatModifiers neutral() {
  const auto &rules = test_ruleset().combat_rules();
  return {rules.modifier_scale, rules.modifier_scale, rules.modifier_scale,
          rules.modifier_scale, rules.modifier_scale};
}

[[nodiscard]] aetheria::rules::CombatInput matchup(std::int32_t power_a,
                                                   std::int32_t power_b) {
  const auto &rules = test_ruleset().combat_rules();
  return {{power_a, neutral(), {}, 0},
          {power_b, neutral(), {}, 0},
          rules.default_exponent,
          1};
}

[[nodiscard]] double signed_relative_error(std::int64_t reference,
                                           std::int64_t candidate) {
  return static_cast<double>(candidate - reference) /
         static_cast<double>(reference);
}

[[nodiscard]] double sample_variance(std::span<const double> samples) {
  const auto mean =
      std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
  double squared{};
  for (const auto sample : samples) {
    const auto difference = sample - mean;
    squared += difference * difference;
  }
  return squared / static_cast<double>(samples.size() - 1U);
}

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

TEST(LocalCombat,
     ThousandBalancedSamplesTrackSiteWithMixedSignsAndHigherVariance) {
  constexpr std::size_t samples = 1'000;
  constexpr std::uint64_t event_base = UINT64_C(0x73c0000000000000);
  auto zone = navigation_zone(kNavigationCenter);
  InMemoryZoneStore store{test_ruleset()};
  ZoneManager manager{store};
  static_cast<void>(manager.adopt(std::move(zone)));
  aetheria::runtime::CrossZoneRuntime runtime{manager};
  std::array<std::int64_t, 2> site_sum{};
  std::array<std::int64_t, 2> local_sum{};
  std::array<std::array<std::int64_t, 2>, 3> site_bin_sum{};
  std::array<std::array<std::int64_t, 2>, 3> local_bin_sum{};
  std::vector<double> site_loss_b;
  std::vector<double> local_loss_b;
  std::array<std::int32_t, 3> ratio_bins{};
  site_loss_b.reserve(samples);
  local_loss_b.reserve(samples);

  for (std::size_t scenario = 0; scenario < samples / 2U; ++scenario) {
    const auto power_a = static_cast<std::int32_t>(75'000 + scenario * 100U);
    const auto combat = matchup(power_a, 100'000);
    const auto &combat_rules = test_ruleset().combat_rules();
    const auto region =
        aetheria::rules::resolve_region_combat(combat, combat_rules);
    const auto ratio = power_a / 10;
    const auto ratio_bin = ratio < 9'000 ? 0U : (ratio <= 11'000 ? 1U : 2U);
    ratio_bins[ratio_bin] += 2;
    for (std::uint64_t member = 0; member < 2; ++member) {
      const auto seed = static_cast<std::uint64_t>(scenario) * 2U + member;
      const auto site = aetheria::site::simulate_site_battle({
          .side_a = combat.side_a,
          .side_b = combat.side_b,
          .region_expected_loss_a = region.loss_a,
          .region_expected_loss_b = region.loss_b,
          .modifier_scale = combat_rules.modifier_scale,
          .event_id = event_base + scenario,
          .sample_seed = seed,
      });
      LocalCombatState state;
      const LocalCombatStats symmetric{70, 20, 10, 2};
      state.combatants = {unit(1, LocalCombatSide::A, {32, 32},
                               Significance::Ambient, symmetric),
                          unit(2, LocalCombatSide::B, {33, 32},
                               Significance::Ambient, symmetric)};
      const auto local_input =
          round_input(site.loss_a, site.loss_b, power_a, 100'000);
      const auto local = aetheria::local::resolve_local_combat_round(
          state, runtime, test_ruleset(), kWestEastBoundary, local_input, seed);
      site_sum[0] += site.loss_a;
      site_sum[1] += site.loss_b;
      local_sum[0] += local.loss[0];
      local_sum[1] += local.loss[1];
      site_bin_sum[ratio_bin][0] += site.loss_a;
      site_bin_sum[ratio_bin][1] += site.loss_b;
      local_bin_sum[ratio_bin][0] += local.loss[0];
      local_bin_sum[ratio_bin][1] += local.loss[1];
      site_loss_b.push_back(site.loss_b);
      local_loss_b.push_back(local.loss[1]);
    }
  }

  const auto error_a = signed_relative_error(site_sum[0], local_sum[0]);
  const auto error_b = signed_relative_error(site_sum[1], local_sum[1]);
  const auto error_total = signed_relative_error(site_sum[0] + site_sum[1],
                                                 local_sum[0] + local_sum[1]);
  std::array<std::array<double, 2>, 3> bin_errors{};
  std::array<bool, 2> has_negative{};
  std::array<bool, 2> has_positive{};
  for (std::size_t bin = 0; bin < bin_errors.size(); ++bin) {
    for (std::size_t side = 0; side < bin_errors[bin].size(); ++side) {
      bin_errors[bin][side] = signed_relative_error(site_bin_sum[bin][side],
                                                    local_bin_sum[bin][side]);
      has_negative[side] = has_negative[side] || bin_errors[bin][side] < 0.0;
      has_positive[side] = has_positive[side] || bin_errors[bin][side] > 0.0;
    }
  }
  const auto site_variance = sample_variance(site_loss_b);
  const auto local_variance = sample_variance(local_loss_b);
  EXPECT_LT(std::abs(error_a), 0.05);
  EXPECT_LT(std::abs(error_b), 0.05);
  EXPECT_LT(std::abs(error_total), 0.05);
  EXPECT_EQ(has_negative, (std::array<bool, 2>{true, true}));
  EXPECT_EQ(has_positive, (std::array<bool, 2>{true, true}));
  EXPECT_GT(local_variance, site_variance);
  EXPECT_EQ(ratio_bins, (std::array<std::int32_t, 3>{300, 402, 298}));
  std::cout << "local_true_site_parity N=1000 R_permyriad=7500..12490 bins="
            << ratio_bins[0] << '/' << ratio_bins[1] << '/' << ratio_bins[2]
            << " mean_A_S_L=" << site_sum[0] / 1000.0 << '/'
            << local_sum[0] / 1000.0 << " mean_B_S_L=" << site_sum[1] / 1000.0
            << '/' << local_sum[1] / 1000.0
            << " signed_errors_A_B_total=" << error_a << '/' << error_b << '/'
            << error_total
            << " signed_errors_bins_low_mid_high_A_B=" << bin_errors[0][0]
            << ',' << bin_errors[0][1] << '/' << bin_errors[1][0] << ','
            << bin_errors[1][1] << '/' << bin_errors[2][0] << ','
            << bin_errors[2][1] << " variance_S_L=" << site_variance << '/'
            << local_variance << '\n';
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

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

} // namespace

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

#include "tests/world/combat_scaling_test_support.h"

namespace {

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

using namespace aetheria::tests::combat_scaling;

[[nodiscard]] double welch_normal_p_value(std::span<const double> first,
                                          std::span<const double> second) {
    const auto mean_first = std::accumulate(first.begin(), first.end(), 0.0) / first.size();
    const auto mean_second = std::accumulate(second.begin(), second.end(), 0.0) / second.size();
    const auto standard_error = std::sqrt(sample_variance(first) / first.size() +
                                          sample_variance(second) / second.size());
    if (standard_error == 0.0) {
        return mean_first == mean_second ? 1.0 : 0.0;
    }
    const auto statistic = std::abs(mean_first - mean_second) / standard_error;
    // N=1000 時 Welch t 的雙尾機率以常態尾端近似；門檻 alpha=0.01。
    return std::erfc(statistic / std::sqrt(2.0));
}

TEST(CombatScaling, MidBattlePromotionIsUnbiasedByWelchMeanTest) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    const auto input = matchup(100'000, 100'000);
    std::vector<double> all_region;
    std::vector<double> promoted;
    all_region.reserve(kCalibrationSamples);
    promoted.reserve(kCalibrationSamples);
    for (std::uint64_t seed = 0; seed < kCalibrationSamples; ++seed) {
        all_region.push_back(aetheria::world::resolve_scaled_combat(
                                 input, rules, CombatLayer::Region, kEventBase, seed)
                                 .loss_b);
        const auto first = aetheria::world::resolve_scaled_combat(
            input, rules, CombatLayer::Region, kEventBase, seed, CombatPhase{0, 2});
        const auto second = aetheria::world::resolve_scaled_combat(
            input, rules, CombatLayer::Local, kEventBase, seed, CombatPhase{1, 2});
        promoted.push_back(first.loss_b + second.loss_b);
    }
    const auto p_value = welch_normal_p_value(all_region, promoted);
    const auto region_mean =
        std::accumulate(all_region.begin(), all_region.end(), 0.0) / all_region.size();
    const auto promoted_mean =
        std::accumulate(promoted.begin(), promoted.end(), 0.0) / promoted.size();
    EXPECT_GT(p_value, 0.01);
    EXPECT_EQ(region_mean, promoted_mean);
    std::cout << "combat_antibrush test=Welch_two_sided_mean alpha=0.01 N=1000"
              << " region_mean=" << region_mean << " promoted_mean=" << promoted_mean
              << " p=" << p_value << '\n';
}

}  // namespace

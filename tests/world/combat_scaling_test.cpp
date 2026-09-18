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

struct CalibrationMetrics {
    std::array<std::int64_t, 3> sum_a{};
    std::array<std::int64_t, 3> sum_b{};
    std::array<std::vector<double>, 3> loss_b_samples;
    std::int32_t ratio_min_permyriad{10'000};
    std::int32_t ratio_max_permyriad{};
    std::array<std::int32_t, 3> ratio_bins{};
};

[[nodiscard]] CalibrationMetrics run_calibration() {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    CalibrationMetrics metrics;
    for (auto& samples : metrics.loss_b_samples) {
        samples.reserve(kCalibrationSamples);
    }
    // 500 個勢均力敵情境各跑兩個 seed；Region/Local 成對反向，Site 則真的跑 cohort 戰術。
    for (std::size_t scenario = 0; scenario < kCalibrationSamples / 2; ++scenario) {
        const auto power_a = static_cast<std::int32_t>(75'000 + scenario * 100);
        const auto input = matchup(power_a, 100'000);
        const auto ratio = power_a / 10;
        metrics.ratio_min_permyriad = std::min(metrics.ratio_min_permyriad, ratio);
        metrics.ratio_max_permyriad = std::max(metrics.ratio_max_permyriad, ratio);
        const auto bin = ratio < 9'000 ? 0U : (ratio <= 11'000 ? 1U : 2U);
        metrics.ratio_bins[bin] += 2;
        for (std::uint64_t pair_member = 0; pair_member < 2; ++pair_member) {
            const auto seed = static_cast<std::uint64_t>(scenario) * 2U + pair_member;
            const auto event_id = kEventBase + scenario;
            for (const auto layer : {CombatLayer::Region, CombatLayer::Site,
                                     CombatLayer::Local}) {
                const auto result = aetheria::world::resolve_scaled_combat(
                    input, rules, layer, event_id, seed);
                const auto index = static_cast<std::size_t>(layer);
                metrics.sum_a[index] += result.loss_a;
                metrics.sum_b[index] += result.loss_b;
                metrics.loss_b_samples[index].push_back(result.loss_b);
            }
        }
    }
    return metrics;
}

[[nodiscard]] double signed_relative_error(std::int64_t reference,
                                           std::int64_t candidate) {
    return static_cast<double>(candidate - reference) /
           static_cast<double>(reference);
}

[[nodiscard]] bool all_same_nonzero_direction(std::span<const double> errors) {
    return std::ranges::all_of(errors, [](double error) { return error > 0.0; }) ||
           std::ranges::all_of(errors, [](double error) { return error < 0.0; });
}

TEST(CombatScaling, ExpectationParityCoversBalancedRatiosWithoutSuppressingVariance) {
    const auto metrics = run_calibration();
    const auto sr_a = signed_relative_error(metrics.sum_a[0], metrics.sum_a[1]);
    const auto ls_a = signed_relative_error(metrics.sum_a[1], metrics.sum_a[2]);
    const auto lr_a = signed_relative_error(metrics.sum_a[0], metrics.sum_a[2]);
    const auto sr_b = signed_relative_error(metrics.sum_b[0], metrics.sum_b[1]);
    const auto ls_b = signed_relative_error(metrics.sum_b[1], metrics.sum_b[2]);
    const auto lr_b = signed_relative_error(metrics.sum_b[0], metrics.sum_b[2]);
    for (const auto error : {sr_a, ls_a, lr_a, sr_b, ls_b, lr_b}) {
        EXPECT_LT(std::abs(error), 0.05);
    }
    const auto region_variance = sample_variance(metrics.loss_b_samples[0]);
    const auto site_variance = sample_variance(metrics.loss_b_samples[1]);
    const auto local_variance = sample_variance(metrics.loss_b_samples[2]);
    const auto sr_total = signed_relative_error(
        metrics.sum_a[0] + metrics.sum_b[0],
        metrics.sum_a[1] + metrics.sum_b[1]);
    const std::array site_region_signs{sr_a, sr_b, sr_total};
    EXPECT_FALSE(all_same_nonzero_direction(site_region_signs))
        << "Site−Region 的 A、B、總損失三組誤差不得全部同向";
    EXPECT_LT(region_variance, site_variance);
    EXPECT_LT(site_variance, local_variance);
    EXPECT_EQ(metrics.ratio_bins, (std::array<std::int32_t, 3>{300, 402, 298}));
    std::cout << "combat_parity samples=1000 seeds=0..999"
              << " region_local_antithetic=1 site_tactical=1"
              << " R_permyriad=" << metrics.ratio_min_permyriad << "/9995/"
              << metrics.ratio_max_permyriad << " bins=" << metrics.ratio_bins[0] << '/'
              << metrics.ratio_bins[1] << '/' << metrics.ratio_bins[2]
              << " mean_A_R_S_L=" << metrics.sum_a[0] / 1'000.0 << '/'
              << metrics.sum_a[1] / 1'000.0 << '/' << metrics.sum_a[2] / 1'000.0
              << " mean_B_R_S_L=" << metrics.sum_b[0] / 1'000.0 << '/'
              << metrics.sum_b[1] / 1'000.0 << '/' << metrics.sum_b[2] / 1'000.0
              << " signed_errors_A_SR_LS_LR=" << sr_a << '/' << ls_a << '/' << lr_a
              << " signed_errors_B_SR_LS_LR=" << sr_b << '/' << ls_b << '/' << lr_b
              << " signed_errors_site_region_A_B_total=" << sr_a << '/' << sr_b << '/'
              << sr_total
              << " variance_R_S_L=" << region_variance << '/' << site_variance << '/'
              << local_variance << '\n';
}

TEST(CombatScaling, FullCalibrationPassFitsCiBudgetUsingMinOfFive) {
    using Clock = std::chrono::steady_clock;
    const auto milliseconds = aetheria::tests::minimum_milliseconds_after_warmup([] {
        const auto start = Clock::now();
        const auto metrics = run_calibration();
        EXPECT_GT(metrics.sum_a[0] + metrics.sum_b[0], 0);
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    });
    EXPECT_LT(milliseconds, 500.0);
    std::cout << "combat_calibration_performance samples=1000 layers=3 warmup=1 measured=5"
              << " minimum_ms=" << milliseconds << " budget_ms=500\n";
}

}  // namespace

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

constexpr std::size_t kCalibrationSamples = 1'000;
constexpr std::uint64_t kEventBase = UINT64_C(0x6d37000000000000);

[[nodiscard]] CombatModifiers neutral(const aetheria::rules::CombatRules& rules) {
    return {rules.modifier_scale, rules.modifier_scale, rules.modifier_scale,
            rules.modifier_scale, rules.modifier_scale};
}

[[nodiscard]] CombatInput matchup(std::int32_t power_a, std::int32_t power_b) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    return {{power_a, neutral(rules), {}, 0},
            {power_b, neutral(rules), {}, 0},
            rules.default_exponent,
            1};
}

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

[[nodiscard]] double sample_variance(std::span<const double> samples) {
    const auto mean = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
    double squared{};
    for (const auto sample : samples) {
        const auto difference = sample - mean;
        squared += difference * difference;
    }
    return squared / static_cast<double>(samples.size() - 1U);
}

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

TEST(CombatScaling, TenPromoteDemoteCyclesConserveStatisticsAndNamedState) {
    CombatEventState cycled{.event_id = 77,
                            .initial_power_a = 100'000,
                            .initial_power_b = 120'000,
                            .accumulated_loss_a = 4'000,
                            .accumulated_loss_b = 7'000,
                            .named = {{1, Significance::Site, 3},
                                      {2, Significance::World, 0}}};
    const auto baseline = cycled;
    for (std::uint64_t cycle = 1; cycle <= 10; ++cycle) {
        static_cast<void>(aetheria::world::promote_combat_event(cycled, 99, 123));
        const aetheria::world::LayerCombatResult no_elapsed_combat{
            .layer = CombatLayer::Local, .resolution_id = cycle};
        EXPECT_TRUE(aetheria::world::demote_combat_event(cycled, no_elapsed_combat));
    }
    EXPECT_EQ(cycled.accumulated_loss_a, baseline.accumulated_loss_a);
    EXPECT_EQ(cycled.accumulated_loss_b, baseline.accumulated_loss_b);
    EXPECT_EQ(cycled.named, baseline.named);
    std::cout << "combat_scale_cycles=10 losses=" << cycled.accumulated_loss_a << '/'
              << cycled.accumulated_loss_b << " named_equal=1\n";
}

TEST(CombatScaling, PromotionLayoutIsBitwiseIdenticalOneHundredTimes) {
    const CombatEventState state{.event_id = 88,
                                 .initial_power_a = 100'000,
                                 .initial_power_b = 120'000,
                                 .accumulated_loss_a = 4'000,
                                 .accumulated_loss_b = 7'000,
                                 .named = {{3, Significance::Region, 2}}};
    const auto first = aetheria::world::promote_combat_event(state, 0x1234, 864'000);
    ASSERT_FALSE(first.placements.empty());
    const auto first_bytes = std::as_bytes(std::span{first.placements});
    for (std::int32_t repeat = 1; repeat < 100; ++repeat) {
        const auto next = aetheria::world::promote_combat_event(state, 0x1234, 864'000);
        EXPECT_TRUE(std::ranges::equal(first_bytes,
                                       std::as_bytes(std::span{next.placements})));
        EXPECT_EQ(next.named, first.named);
    }
    std::cout << "combat_promotion repeats=100 placements=" << first.placements.size()
              << " bitwise_equal=1\n";
}

TEST(CombatScaling, SiteHomeNeverWritesRegionFaceDamage) {
    CombatExecutionCounters counters;
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    const auto result = aetheria::world::resolve_scaled_combat(
        matchup(100'000, 100'000), rules, CombatLayer::Site, 91, 0, {}, {}, &counters);
    CombatEventState state{.event_id = 91,
                           .initial_power_a = 100'000,
                           .initial_power_b = 100'000,
                           .named = {}};
    EXPECT_TRUE(aetheria::world::demote_combat_event(state, result, &counters));
    EXPECT_GT(result.loss_a + result.loss_b, 0);
    EXPECT_EQ(counters.site_face_runs, 1U);
    EXPECT_EQ(counters.region_face_runs, 0U);
    EXPECT_EQ(counters.region_face_damage_writes, 0U);
    EXPECT_EQ(counters.site_reduction_writes, 1U);
    std::cout << "combat_single_home site_runs=" << counters.site_face_runs
              << " region_runs=" << counters.region_face_runs
              << " region_damage_writes=" << counters.region_face_damage_writes
              << " site_reduction_writes=" << counters.site_reduction_writes << '\n';
}

TEST(CombatScaling, AllFiveSignificanceDeltasAreRealAndQuotaConserved) {
    constexpr std::array tiers{Significance::Ambient, Significance::Local,
                               Significance::Site, Significance::Region,
                               Significance::World};
    constexpr std::array expected_limits{0, 1'000, 5'000, 20'000, 100'000};
    std::array<std::int32_t, 5> applied{};
    for (std::size_t index = 0; index < tiers.size(); ++index) {
        const auto allocation = aetheria::world::apply_personal_contribution(
            12'000, 100'000, PersonalContribution{tiers[index], 1'000'000});
        EXPECT_EQ(allocation.delta_limit, expected_limits[index]);
        EXPECT_EQ(allocation.named_loss + allocation.unnamed_loss,
                  allocation.final_loss);
        applied[index] = allocation.applied_deviation;
    }
    EXPECT_EQ(applied[0], 0);
    EXPECT_GT(applied[1], 0);
    EXPECT_GT(applied[2], applied[1]);
    EXPECT_GT(applied[3], applied[2]);
    EXPECT_GT(applied[4], applied[3]);
    std::cout << "combat_delta limits_A_L_S_R_W=" << expected_limits[0] << '/'
              << expected_limits[1] << '/' << expected_limits[2] << '/'
              << expected_limits[3] << '/' << expected_limits[4]
              << " applied=" << applied[0] << '/' << applied[1] << '/' << applied[2]
              << '/' << applied[3] << '/' << applied[4] << " quota_conserved=1\n";
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

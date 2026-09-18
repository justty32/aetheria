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

#include "tests/world/faction_ai_test_support.h"

namespace {

using namespace aetheria;
using tests::test_ruleset;
using world::FactionId;
using world::WorldDiplomacyState;

using namespace aetheria::tests::faction_ai;



TEST(FactionAiLod, ThreeLevelsRunWithSeparatedCostsAndUnbiasedPowerCurves) {
    std::int64_t full_total{};
    std::int64_t simplified_total{};
    std::int64_t statistical_total{};
    std::size_t full_count{};
    std::size_t simplified_count{};
    std::size_t statistical_count{};
    std::size_t positive_simplified{};
    std::size_t negative_simplified{};
    std::size_t positive_statistical{};
    std::size_t negative_statistical{};
    std::size_t full_cost{};
    std::size_t simplified_cost{};
    std::size_t statistical_cost{};
    constexpr std::size_t samples = 24;
    for (std::size_t sample = 0; sample < samples; ++sample) {
        auto world = observed_world(1000 + sample);
        world.faction_mind(FactionId{1}).forced_goal = ai::FactionGoal::Prosper;
        for (std::int64_t xun = 0; xun < 1000; ++xun) {
            const auto now = time::Tick{xun * static_cast<std::int64_t>(time::kXun)};
            const auto full = world::advance_faction_ai_xun(
                world, FactionId{1}, {3, 0, true, true, false}, now, test_ruleset());
            const auto simplified = world::advance_faction_ai_xun(
                world, FactionId{2}, {0, 10, true, false, false}, now, test_ruleset());
            const auto statistical = world::advance_faction_ai_xun(
                world, FactionId{3}, {0, 10, false, false, false}, now, test_ruleset());
            ++full_count;
            ++simplified_count;
            ++statistical_count;
            full_cost += full.utility_evaluations;
            simplified_cost += simplified.utility_evaluations;
            statistical_cost += statistical.utility_evaluations;
        }
        const auto full_power = *world.faction_truth(FactionId{1});
        const auto simplified_power = *world.faction_truth(FactionId{2});
        const auto statistical_power = *world.faction_truth(FactionId{3});
        const auto full_value = static_cast<std::int64_t>(full_power.military_power) +
                                full_power.economic_power;
        const auto simplified_value =
            static_cast<std::int64_t>(simplified_power.military_power) +
            simplified_power.economic_power;
        const auto statistical_value =
            static_cast<std::int64_t>(statistical_power.military_power) +
            statistical_power.economic_power;
        full_total += full_value;
        simplified_total += simplified_value;
        statistical_total += statistical_value;
        positive_simplified += simplified_value > full_value ? 1U : 0U;
        negative_simplified += simplified_value < full_value ? 1U : 0U;
        positive_statistical += statistical_value > full_value ? 1U : 0U;
        negative_statistical += statistical_value < full_value ? 1U : 0U;
    }
    const auto simplified_error =
        (simplified_total - full_total) * 10000 / full_total;
    const auto statistical_error =
        (statistical_total - full_total) * 10000 / full_total;
    EXPECT_LT(std::abs(simplified_error), 500);
    EXPECT_LT(std::abs(statistical_error), 500);
    EXPECT_GT(positive_simplified, 0U);
    EXPECT_GT(negative_simplified, 0U);
    EXPECT_GT(positive_statistical, 0U);
    EXPECT_GT(negative_statistical, 0U);
    EXPECT_GT(full_cost / full_count, simplified_cost / simplified_count);
    EXPECT_GT(simplified_cost / simplified_count, statistical_cost / statistical_count);
    std::cout << "faction_ai lod counts=" << full_count << ',' << simplified_count << ','
              << statistical_count << " cost_per_xun=" << full_cost / full_count << ','
              << simplified_cost / simplified_count << ','
              << statistical_cost / statistical_count
              << " relative_error_permyriad simplified=" << simplified_error
              << " statistical=" << statistical_error << " signs simplified=+"
              << positive_simplified << "/-" << negative_simplified << " statistical=+"
              << positive_statistical << "/-" << negative_statistical << '\n';
}

TEST(FactionAiLod, AllFactionEntryUsesFieldAndRunsEveryFactionOnce) {
    auto world = observed_world(31337);
    const std::array attention{
        world::FactionAttention{},
        world::FactionAttention{0, 100, true, true, false},
        world::FactionAttention{0, 100, true, false, false},
        world::FactionAttention{0, 100, false, false, false},
    };
    const auto reports = world::advance_all_factions_ai_xun(
        world, attention, time::Tick{0}, test_ruleset());
    ASSERT_EQ(reports.size(), 3U);
    EXPECT_EQ(reports[0].lod, ai::FactionAiLod::Full);
    EXPECT_EQ(reports[1].lod, ai::FactionAiLod::Simplified);
    EXPECT_EQ(reports[2].lod, ai::FactionAiLod::Statistical);
    EXPECT_EQ(reports[0].decision.command.issuer, 1);
    EXPECT_EQ(reports[1].decision.command.issuer, 2);
    EXPECT_EQ(reports[2].decision.command.issuer, 3);
}

TEST(FactionAiPerformance, AllFactionDecisionsAreMillisecondScaleMinOfFive) {
    auto measure = [] {
        const auto started = std::chrono::steady_clock::now();
        for (std::uint64_t batch = 0; batch < 5; ++batch) {
            auto world = observed_world(9000 + batch);
            for (std::uint16_t faction = 1; faction <= 3; ++faction) {
                static_cast<void>(world::plan_faction_ai_xun(
                    world, FactionId{faction}, {3, 0, true, true, false},
                    time::Tick{0}, test_ruleset()));
            }
        }
        const auto elapsed = std::chrono::steady_clock::now() - started;
        return std::chrono::duration<double, std::milli>(elapsed).count();
    };
    const auto minimum = tests::minimum_milliseconds_after_warmup(measure);
    EXPECT_LT(minimum, 10.0);
    std::cout << "faction_ai perf factions=15 samples="
              << tests::kPerformanceSampleCount << " minimum_ms=" << minimum << '\n';
}

}  // namespace

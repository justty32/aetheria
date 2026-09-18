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

}  // namespace

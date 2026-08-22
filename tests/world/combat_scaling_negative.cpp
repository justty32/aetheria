#include "core/world/combat_scaling.h"
#include "tests/support/ruleset_fixture.h"

#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>

namespace {

[[nodiscard]] aetheria::rules::CombatInput matchup() {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    const aetheria::rules::CombatModifiers neutral{
        rules.modifier_scale, rules.modifier_scale, rules.modifier_scale,
        rules.modifier_scale, rules.modifier_scale};
    return {{100'000, neutral, {}, 0},
            {100'000, neutral, {}, 0},
            rules.default_exponent,
            1};
}

TEST(CombatScalingNegative, RemovingDeltaClampBreaksPromotionUnbiasedness) {
    auto scaling = aetheria::world::CombatScalingRules{};
    scaling.random_spread_permyriad = {0, 0, 0};
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    const auto region = aetheria::world::resolve_scaled_combat(
        matchup(), rules, aetheria::world::CombatLayer::Region, 1, 0, {}, {}, nullptr,
        scaling);
    // 故障注入：Local 個人貢獻直接相加，刻意拿掉 significance clamp 與軍力上限。
    const auto local_unclamped = region.loss_b + 1'000'000;
    std::cout << "delta_clamp_negative region_loss=" << region.loss_b
              << " local_unclamped=" << local_unclamped << " army_power=100000\n";
    EXPECT_EQ(local_unclamped, region.loss_b)
        << "縮放無偏已破：一個 Ambient 個體能打贏整支軍隊";
}

TEST(CombatScalingNegative, SettlingRegionFaceBesideSiteIsDetected) {
    aetheria::world::CombatExecutionCounters counters;
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    static_cast<void>(aetheria::world::resolve_scaled_combat(
        matchup(), rules, aetheria::world::CombatLayer::Site, 2, 0, {}, {}, &counters));
    // 故障注入：主場已是 Site，仍再跑一次 Region 面孔。
    static_cast<void>(aetheria::world::resolve_scaled_combat(
        matchup(), rules, aetheria::world::CombatLayer::Region, 2, 0, {}, {}, &counters));
    std::cout << "double_settlement_negative site_runs=" << counters.site_face_runs
              << " region_damage_writes=" << counters.region_face_damage_writes << '\n';
    EXPECT_EQ(counters.region_face_damage_writes, 0U) << "Site 主場不得重算 Region 損傷";
}

}  // namespace

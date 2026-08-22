#include "core/world/combat_scaling.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
#include <cstdint>
#include <array>
#include <cmath>
#include <iostream>

#include <gtest/gtest.h>

namespace {

[[nodiscard]] aetheria::rules::CombatInput matchup(std::int32_t power_a = 100'000,
                                                   std::int32_t power_b = 100'000) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    const aetheria::rules::CombatModifiers neutral{
        rules.modifier_scale, rules.modifier_scale, rules.modifier_scale,
        rules.modifier_scale, rules.modifier_scale};
    return {{power_a, neutral, {}, 0},
            {power_b, neutral, {}, 0},
            rules.default_exponent,
            1};
}

[[nodiscard]] double signed_error(std::int64_t reference, std::int64_t candidate) {
    return static_cast<double>(candidate - reference) / reference;
}

TEST(CombatScalingNegative, ThreePercentSiteBiasPassesMagnitudeButFailsSignGuard) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    auto scaling = aetheria::world::CombatScalingRules{};
    scaling.site_systematic_bias_permyriad = 300;
    std::int64_t region_a{};
    std::int64_t region_b{};
    std::int64_t site_a{};
    std::int64_t site_b{};
    for (std::uint64_t seed = 0; seed < 1'000; ++seed) {
        const auto input = matchup();
        const auto region = aetheria::world::resolve_scaled_combat(
            input, rules, aetheria::world::CombatLayer::Region, 3, seed);
        const auto site = aetheria::world::resolve_scaled_combat(
            input, rules, aetheria::world::CombatLayer::Site, 3, seed, {}, {}, nullptr,
            scaling);
        region_a += region.loss_a;
        region_b += region.loss_b;
        site_a += site.loss_a;
        site_b += site.loss_b;
    }
    const std::array errors{
        signed_error(region_a, site_a), signed_error(region_b, site_b),
        signed_error(region_a + region_b, site_a + site_b)};
    const auto magnitude_under_five =
        std::ranges::all_of(errors, [](double error) { return std::abs(error) < 0.05; });
    const auto all_positive =
        std::ranges::all_of(errors, [](double error) { return error > 0.0; });
    std::cout << "site_bias_negative signed_errors_A_B_total=" << errors[0] << '/'
              << errors[1] << '/' << errors[2]
              << " absolute_under_5=" << magnitude_under_five
              << " all_positive=" << all_positive << '\n';
    EXPECT_TRUE(magnitude_under_five);
    EXPECT_FALSE(all_positive)
        << "符號守門抓到系統性 +3%：A、B、總損失三組全為正";
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

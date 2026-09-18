#include "core/rules/combat.h"
#include "core/rules/ruleset.h"
#include "tests/support/ruleset_fixture.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <numeric>

#include <gtest/gtest.h>

#include "tests/rules/combat_test_support.h"

namespace {

using namespace aetheria::rules;
using aetheria::world::Significance;

using namespace aetheria::tests::combat_support;

TEST(CombatCalibration, LegendaryVictoryAndRetreatHaveAVisibleInterval) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    std::int32_t largest_routed_militia{};
    std::int32_t first_legendary_retreat{};
    for (std::int32_t militia = 1; militia <= 2'000; ++militia) {
        const auto result = resolve_region_combat(matchup(25'600, militia * 100, rules), rules);
        if (result.outcome == Outcome::SideBRouted) {
            largest_routed_militia = militia;
        }
        if (first_legendary_retreat == 0 && result.outcome == Outcome::SideARouted) {
            first_legendary_retreat = militia;
        }
    }
    ASSERT_GT(largest_routed_militia, 0);
    ASSERT_GT(first_legendary_retreat, 0);
    EXPECT_GT(first_legendary_retreat - largest_routed_militia, 50);
    EXPECT_EQ(apply_individual_tier_gate(25'600, Significance::Ambient, Significance::World, true,
                                        aetheria::tests::test_ruleset().power_rules()),
              0);
    std::cout << "M6.3 legendary interval: N=" << largest_routed_militia
              << " M=" << first_legendary_retreat << '\n';
}

TEST(CombatCalibration, RandomThousandCampaignsRarelyAnnihilate) {
    const auto& rules = aetheria::tests::test_ruleset().combat_rules();
    std::uint32_t state = 0x6a09e667U;
    std::int32_t annihilated_campaigns{};
    const auto next = [&state]() {
        state = state * 1'664'525U + 1'013'904'223U;
        return state;
    };
    for (std::int32_t sample = 0; sample < 1'000; ++sample) {
        auto input = matchup(static_cast<std::int32_t>(10'000 + next() % 990'001U),
                             static_cast<std::int32_t>(10'000 + next() % 990'001U), rules);
        input.side_a.modifiers.supply =
            rules.supply.minimum + static_cast<std::int32_t>(next() % 801U);
        input.side_b.modifiers.supply =
            rules.supply.minimum + static_cast<std::int32_t>(next() % 801U);
        input.side_a.modifiers.morale =
            rules.morale.minimum + static_cast<std::int32_t>(next() % 801U);
        input.side_b.modifiers.morale =
            rules.morale.minimum + static_cast<std::int32_t>(next() % 801U);
        const auto result = resolve_region_combat(input, rules);
        annihilated_campaigns +=
            result.loss_a == input.side_a.power || result.loss_b == input.side_b.power ? 1 : 0;
    }
    EXPECT_LE(annihilated_campaigns, 200);
    std::cout << "M6.3 annihilated-campaigns-per-1000=" << annihilated_campaigns << '\n';
}

}  // namespace

#include "core/world/diplomacy.h"
#include "tests/support/ruleset_fixture.h"

#include <aetheria/ai/faction_view.h>

#include <array>
#include <concepts>
#include <cstdint>
#include <iostream>
#include <type_traits>

#include <gtest/gtest.h>

#include "tests/world/diplomacy_test_support.h"

namespace {

using aetheria::ai::FactionView;
using aetheria::rules::Ruleset;
using aetheria::tests::test_ruleset;
using aetheria::time::kXun;
using aetheria::time::Tick;
using aetheria::world::DiplomaticRelation;
using aetheria::world::FactionId;
using aetheria::world::WorldDiplomacyState;

using namespace aetheria::tests::diplomacy;



TEST(DiplomacyWar, UnjustifiedWarDamagesEveryThirdPartysTrustTowardAggressor) {
    auto state = world();
    static_cast<void>(
        state.declare_war(FactionId{1}, FactionId{2}, std::nullopt, Tick{0}));
    EXPECT_EQ(state.relation(FactionId{3}, FactionId{1}).trust,
              test_ruleset().diplomacy_rules().unjustified_war_trust_penalty);
    EXPECT_EQ(state.relation(FactionId{1}, FactionId{3}).trust, 0);
    EXPECT_EQ(state.relation(FactionId{2}, FactionId{1}).trust, 0);
}

TEST(DiplomacyWar, EndlessWarWearinessMonotonicallyReachesPeacePressure) {
    auto state = world();
    auto& war =
        state.declare_war(FactionId{1}, FactionId{2}, std::nullopt, Tick{0});
    std::int32_t previous{};
    std::size_t crossing_xun{};
    for (std::size_t xun = 1; xun <= 200; ++xun) {
        state.advance_war_xun(war, {0, 0});
        EXPECT_GT(war.weariness[0], previous);
        previous = war.weariness[0];
        if (state.peace_pressure_reached(war, 0)) {
            crossing_xun = xun;
            break;
        }
    }
    std::cout << "war_weariness crossing_xun=" << crossing_xun
              << " value=" << war.weariness[0] << " threshold="
              << test_ruleset().diplomacy_rules().war_weariness.peace_threshold
              << '\n';
    EXPECT_GT(crossing_xun, 3U);
    EXPECT_LT(crossing_xun, 200U);
}

TEST(DiplomacyWar, CasualtiesRaiseWearinessAboveTimeOnlyProgression) {
    auto state = world();
    auto& war =
        state.declare_war(FactionId{1}, FactionId{2}, std::nullopt, Tick{0});
    state.advance_war_xun(war, {1000, 0});
    EXPECT_GT(war.weariness[0], war.weariness[1]);
}

TEST(DiplomacyPeace, PlayerAndAiPathsCallTheSamePublicIntegerFormula) {
    auto state = world();
    auto& war =
        state.declare_war(FactionId{1}, FactionId{2}, std::nullopt, Tick{0});
    state.add_war_score(war, 500);
    war.weariness = {200, 700};
    constexpr std::int32_t pressure = 100;
    const auto input = aetheria::diplomacy::PeaceLeverageInput{
        .war_score = war.war_score,
        .own_weariness = war.weariness[0],
        .opponent_weariness = war.weariness[1],
        .third_party_pressure = pressure,
    };
    const auto weights = test_ruleset().diplomacy_rules().peace_weights;
    const auto player =
        state.player_peace_leverage(war, FactionId{1}, pressure);
    const auto ai = aetheria::ai::ai_peace_leverage(input, weights);
    EXPECT_EQ(player, ai);
    EXPECT_EQ(state.peace_terms(player),
              aetheria::diplomacy::PeaceTerms::CedeTerritory);
    std::cout << "peace_leverage player=" << player << " ai=" << ai << '\n';
}

}  // namespace

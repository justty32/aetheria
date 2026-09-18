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

template <typename View>
concept ExposesWorldTruth = requires(const View& view) { view.world(); };

static_assert(!ExposesWorldTruth<FactionView>);

static_assert(std::is_integral_v<decltype(DiplomaticRelation::favor)>);

static_assert(std::is_integral_v<
              decltype(aetheria::diplomacy::PeaceLeverageInput::war_score)>);

static_assert(!std::is_floating_point_v<decltype(DiplomaticRelation::trust)>);

TEST(DiplomacyRelations,
     MatrixIsDirectedAndFourComponentsRevertAtOrderedRates) {
    auto state = world();
    constexpr auto a = FactionId{1};
    constexpr auto b = FactionId{2};
    state.set_relation(
        a, b, {.favor = 1000, .trust = 1000, .fear = 1000, .grievance = 1000});

    const auto reverse_initial = state.relation(b, a);
    std::cout << "directed_reverse initial=" << reverse_initial.favor << ','
              << reverse_initial.trust << ',' << reverse_initial.fear << ','
              << reverse_initial.grievance << '\n';
    EXPECT_EQ(reverse_initial, DiplomaticRelation{});
    for (int xun = 0; xun < 10; ++xun) {
        state.advance_relations_xun();
    }

    const auto result = state.relation(a, b);
    const std::array returns{1000 - result.favor, 1000 - result.trust,
                             1000 - result.fear, 1000 - result.grievance};
    std::cout << "diplomacy_return favor=" << returns[0]
              << " trust=" << returns[1] << " fear=" << returns[2]
              << " grievance=" << returns[3] << '\n';
    EXPECT_GT(returns[0], returns[2]);
    EXPECT_GT(returns[2], returns[1]);
    EXPECT_GT(returns[1], returns[3]);
    const auto reverse_after = state.relation(b, a);
    std::cout << "directed_reverse after_10=" << reverse_after.favor << ','
              << reverse_after.trust << ',' << reverse_after.fear << ','
              << reverse_after.grievance << '\n';
    EXPECT_EQ(reverse_after, DiplomaticRelation{});
}

TEST(DiplomacyTreaties, DurationZeroIsIndefiniteAndFiniteTreatyGetsExpiry) {
    auto state = world();
    const auto peace = *test_ruleset().find_treaty("treaty.peace");
    const auto marriage = *test_ruleset().find_treaty("treaty.marriage");
    const auto& finite =
        state.start_treaty(peace, FactionId{1}, FactionId{2}, Tick{0});
    ASSERT_TRUE(finite.expires_at.has_value());
    EXPECT_EQ(*finite.expires_at, Tick{72 * static_cast<std::int64_t>(kXun)});
    const auto& indefinite =
        state.start_treaty(marriage, FactionId{1}, FactionId{2}, Tick{0});
    EXPECT_FALSE(indefinite.expires_at.has_value());
}

TEST(DiplomacyCasusBelli, AvailabilityChangesAtDataDrivenExpiryBoundary) {
    auto state = world();
    const auto revenge =
        *test_ruleset().find_casus_belli("casus_belli.revenge");
    const auto& claim =
        state.grant_casus_belli(FactionId{1}, FactionId{2}, revenge, Tick{0});
    const auto before = claim.expires_at - kXun;
    EXPECT_TRUE(state.has_casus_belli(FactionId{1}, FactionId{2}, revenge,
                                      Tick{static_cast<std::int64_t>(before)}));
    EXPECT_FALSE(state.has_casus_belli(FactionId{1}, FactionId{2}, revenge,
                                       claim.expires_at));
    std::cout << "casus_belli expiry_xun="
              << static_cast<std::int64_t>(claim.expires_at) /
                     static_cast<std::int64_t>(kXun)
              << " before=1 at_expiry=0\n";
}

TEST(FactionKnowledge,
     SameSeedAndObservationsProduceSameViewWithoutWorldBackReference) {
    auto first = world(12345);
    auto second = world(12345);
    for (auto* state : {&first, &second}) {
        state->set_faction_truth(FactionId{1}, 900, 700);
        state->set_faction_truth(FactionId{2}, 1200, 800);
        state->set_faction_truth(FactionId{3}, 500, 1300);
        state->set_relation(
            FactionId{1}, FactionId{2},
            {.favor = -100, .trust = 200, .fear = 600, .grievance = 400});
        state->observe_faction(FactionId{1}, FactionId{2}, 2000, Tick{10});
        state->observe_faction(FactionId{1}, FactionId{3}, 2000, Tick{10});
    }

    const auto first_view =
        aetheria::world::make_faction_view(first, FactionId{1});
    const auto second_view =
        aetheria::world::make_faction_view(second, FactionId{1});
    ASSERT_EQ(first_view.estimates().size(), 2U);
    ASSERT_EQ(first_view.estimates().size(), second_view.estimates().size());
    EXPECT_EQ(first_view.own_military_power(), 900);
    EXPECT_EQ(first_view.estimates()[0], second_view.estimates()[0]);
    EXPECT_EQ(first_view.estimates()[1], second_view.estimates()[1]);
    EXPECT_EQ(first_view.estimate(2)->relation.fear, 600);
    std::cout << "faction_view seed=12345 target2="
              << first_view.estimates()[0].military_power << ','
              << first_view.estimates()[0].economic_power
              << " target3=" << first_view.estimates()[1].military_power << ','
              << first_view.estimates()[1].economic_power << '\n';
}

}  // namespace

#include "core/narrative/emergent_quest.h"

#include "core/site/site_build_loop.h"
#include "core/site/site_materialize.h"
#include "core/site/site_reduction.h"
#include "tests/site/site_reduction_test_support.h"

#include <array>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::narrative::EmergentQuestKind;
using aetheria::narrative::NarrativeWorldSnapshot;
using aetheria::tests::kReductionCoordinate;
using aetheria::tests::kReductionRegionId;
using aetheria::tests::kReductionWorldSeed;
using aetheria::tests::reduction_region;
using aetheria::tests::test_ruleset;

[[nodiscard]] NarrativeWorldSnapshot meaningful_needs() {
    NarrativeWorldSnapshot state;
    state.cities = {
        {kReductionCoordinate, "place.stonebridge", 40, 100},
        {{0, 1}, "place.full_granary", 100, 100},
    };
    state.tiles = {
        {{4, 5}, "place.old_road", 20, 60},
        {{5, 5}, "place.safe_road", 60, 60},
    };
    state.named_npcs = {
        {41, "person.martha_grocer", "place.stonebridge", true, true, true},
        {42, "person.unknown", "place.stonebridge", false, true, true},
        {43, "person.glen_veteran", "place.stonebridge", true, true, false},
    };
    state.faction_tensions = {
        {1, 2, "faction.ember", "faction.river", 800, 700, 1000, 920, 15},
        {1, 3, "faction.ember", "faction.mountain", 900, 700, 1000, 400, 15},
    };
    state.dungeons = {
        {91, "place.deep_ruin", false, 8, 6},
        {92, "place.cleared_ruin", true, 9, 6},
        {93, "place.shallow_cave", false, 2, 6},
    };
    return state;
}

TEST(EmergentQuest, FiveKindsComeOnlyFromMatchingWorldNeeds) {
    const auto quests = aetheria::narrative::detect_emergent_quests(meaningful_needs());
    std::array<std::uint32_t, 5> counts{};
    for (const auto& quest : quests) {
        ++counts.at(static_cast<std::size_t>(quest.kind));
        EXPECT_FALSE(quest.title_key.empty());
        EXPECT_FALSE(quest.description_key.empty());
        EXPECT_FALSE(quest.subject_name_key.empty());
        std::cout << "emergent_quest kind=" << static_cast<std::uint32_t>(quest.kind)
                  << " subject=" << quest.subject_name_key
                  << " observed=" << quest.observed_value
                  << " required=" << quest.required_value << '\n';
    }
    EXPECT_EQ(counts, (std::array<std::uint32_t, 5>{1, 1, 1, 1, 1}));
    std::cout << "emergent_counts food=" << counts[0] << " bandit=" << counts[1]
              << " missing=" << counts[2] << " intel=" << counts[3]
              << " dungeon=" << counts[4] << '\n';
}

TEST(EmergentQuest, FoodDeliveryChangesTheObservedCityThroughOneReductionWrite) {
    auto tiles = reduction_region();
    auto live_site = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, test_ruleset());
    aetheria::site::enter_full_site(live_site, tiles, kReductionCoordinate);
    auto& economy = aetheria::site::city_build_state(live_site).economy;
    economy.food_stock = 40;
    aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, live_site);

    const auto quests = aetheria::narrative::detect_emergent_quests(meaningful_needs());
    const auto delivery = std::ranges::find(quests, EmergentQuestKind::FoodDelivery,
                                            &aetheria::narrative::EmergentQuest::kind);
    ASSERT_NE(delivery, quests.end());
    const auto report =
        aetheria::narrative::complete_food_delivery(*delivery, tiles, live_site);

    EXPECT_EQ(report.food_before, 40U);
    EXPECT_EQ(report.delivered, 60U);
    EXPECT_EQ(report.food_after, 100U);
    EXPECT_EQ(report.reduction_writes, 1U);
    EXPECT_EQ(tiles.reduction_value<aetheria::world::FoodStockReduction>(kReductionCoordinate),
              100U);
    std::cout << "food_delivery before=" << report.food_before
              << " delivered=" << report.delivered << " after=" << report.food_after
              << " reduction_writes=" << report.reduction_writes << '\n';
}

TEST(EmergentQuest, FakeOrStaleNeedCannotChangeAnotherWorldLocation) {
    auto tiles = reduction_region();
    auto live_site = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, test_ruleset());
    aetheria::site::enter_full_site(live_site, tiles, kReductionCoordinate);
    aetheria::site::city_build_state(live_site).economy.food_stock = 55;
    aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, live_site);

    const auto quests = aetheria::narrative::detect_emergent_quests(meaningful_needs());
    const auto delivery = std::ranges::find(quests, EmergentQuestKind::FoodDelivery,
                                            &aetheria::narrative::EmergentQuest::kind);
    ASSERT_NE(delivery, quests.end());
    EXPECT_THROW(static_cast<void>(
                     aetheria::narrative::complete_food_delivery(*delivery, tiles, live_site)),
                 std::logic_error);
    EXPECT_EQ(tiles.reduction_value<aetheria::world::FoodStockReduction>(kReductionCoordinate),
              55U);
}

}  // namespace

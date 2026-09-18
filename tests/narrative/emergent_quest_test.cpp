#include "core/narrative/emergent_quest.h"

#include "core/serialize/zone_codec.h"
#include "core/site/site_build_loop.h"
#include "core/site/site_materialize.h"
#include "core/site/site_reduction.h"
#include "tests/site/site_reduction_test_support.h"

#include <array>
#include <iostream>
#include <ranges>

#include <gtest/gtest.h>

#include "tests/narrative/emergent_quest_test_support.h"

namespace {

using aetheria::narrative::EmergentQuestKind;
using aetheria::narrative::NarrativeWorldSnapshot;
using aetheria::narrative::NarrativeWorldView;
using aetheria::tests::kReductionCoordinate;
using aetheria::tests::kReductionRegionId;
using aetheria::tests::kReductionWorldSeed;
using aetheria::tests::reduction_region;
using aetheria::tests::test_ruleset;

using namespace aetheria::tests::emergent_quest;

[[nodiscard]] std::array<std::uint32_t, 5>
count_kinds(const std::vector<aetheria::narrative::EmergentQuest>& quests) {
    std::array<std::uint32_t, 5> counts{};
    for (const auto& quest : quests) {
        ++counts.at(static_cast<std::size_t>(quest.kind));
    }
    return counts;
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

TEST(EmergentQuest, FourNonDiplomaticKindsReadOnlyAuthoritativeWorldState) {
    auto tiles = reduction_region();
    auto live_site = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, test_ruleset());
    aetheria::site::enter_full_site(live_site, tiles, kReductionCoordinate);
    install_real_observations(live_site);
    aetheria::site::city_build_state(live_site).economy.food_stock = 40;
    aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, live_site, test_ruleset());

    const std::array<const aetheria::zone::Zone*, 1> loaded_sites{&live_site};
    const NarrativeWorldView view{&tiles, loaded_sites, {}};
    const auto first = aetheria::narrative::detect_emergent_quests(view, test_ruleset());
    const auto repeat = aetheria::narrative::detect_emergent_quests(view, test_ruleset());
    EXPECT_EQ(first, repeat);
    const auto counts = count_kinds(first);
    EXPECT_EQ(counts, (std::array<std::uint32_t, 5>{1, 1, 1, 0, 1}));
    EXPECT_EQ(tiles.reduction_value<aetheria::world::OrderReduction>(kReductionCoordinate), 20U);
    std::cout << "world_truth_counts food=" << counts[0] << " bandit=" << counts[1]
              << " missing=" << counts[2] << " intel_interface_only=" << counts[3]
              << " dungeon=" << counts[4] << " repeat_equal=" << (first == repeat) << " order=20\n";
}

TEST(EmergentQuest, FakeOrStaleNeedCannotChangeAnotherWorldLocation) {
    auto tiles = reduction_region();
    auto live_site = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, test_ruleset());
    aetheria::site::enter_full_site(live_site, tiles, kReductionCoordinate);
    aetheria::site::city_build_state(live_site).economy.food_stock = 55;
    aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, live_site, test_ruleset());

    const auto quests = aetheria::narrative::detect_emergent_quests(meaningful_needs());
    const auto delivery = std::ranges::find(quests, EmergentQuestKind::FoodDelivery,
                                            &aetheria::narrative::EmergentQuest::kind);
    ASSERT_NE(delivery, quests.end());
    EXPECT_THROW(static_cast<void>(aetheria::narrative::complete_food_delivery(
                     *delivery, tiles, live_site, test_ruleset())),
                 std::logic_error);
    EXPECT_EQ(tiles.reduction_value<aetheria::world::FoodStockReduction>(kReductionCoordinate),
              55U);
}

}  // namespace

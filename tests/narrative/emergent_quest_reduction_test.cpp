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



TEST(EmergentQuest, BanditSuppressionImprovesOrderThroughOneReductionWrite) {
    auto tiles = reduction_region();
    auto live_site = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, test_ruleset());
    aetheria::site::enter_full_site(live_site, tiles, kReductionCoordinate);
    install_real_observations(live_site);
    aetheria::site::city_build_state(live_site).economy.food_stock = 100;
    aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, live_site, test_ruleset());
    const std::array<const aetheria::zone::Zone*, 1> loaded_sites{&live_site};
    const NarrativeWorldView view{&tiles, loaded_sites, {}};
    const auto quests = aetheria::narrative::detect_emergent_quests(view, test_ruleset());
    const auto suppression = std::ranges::find(quests, EmergentQuestKind::BanditSuppression,
                                               &aetheria::narrative::EmergentQuest::kind);
    ASSERT_NE(suppression, quests.end());

    const auto report = aetheria::narrative::complete_bandit_suppression(*suppression, tiles,
                                                                         live_site, test_ruleset());
    EXPECT_EQ(report.order_before, 20U);
    EXPECT_EQ(report.order_after, 50U);
    EXPECT_EQ(report.bandit_pressure_before, 40U);
    EXPECT_EQ(report.bandit_pressure_after, 10U);
    EXPECT_EQ(report.reduction_writes, 1U);
    EXPECT_EQ(tiles.reduction_value<aetheria::world::OrderReduction>(kReductionCoordinate),
              report.order_after);
    EXPECT_TRUE(std::ranges::none_of(
        aetheria::narrative::detect_emergent_quests(view, test_ruleset()),
        [](const auto& quest) { return quest.kind == EmergentQuestKind::BanditSuppression; }));
    std::cout << "bandit_suppression order_before=" << report.order_before
              << " order_after=" << report.order_after
              << " bandit_pressure_before=" << report.bandit_pressure_before
              << " bandit_pressure_after=" << report.bandit_pressure_after
              << " reduction_writes=" << report.reduction_writes << '\n';
}

TEST(EmergentQuest, OrderMissingDungeonClearedAndDepthSurviveSaveRoundTrip) {
    auto tiles = reduction_region();
    auto live_site = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, test_ruleset());
    install_real_observations(live_site);
    auto& persistent = std::get<aetheria::zone::SitePayload>(live_site.payload).layers.persistent;
    persistent.order->bandit_pressure = 17;
    persistent.named_npcs.front().missing = true;
    persistent.dungeons.front().cleared = true;
    aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, live_site, test_ruleset());
    const auto saved_order =
        tiles.reduction_value<aetheria::world::OrderReduction>(kReductionCoordinate);
    const auto saved_depth = persistent.dungeons.front().depth;

    const auto site_bytes = aetheria::serialize::encode_zone(live_site, test_ruleset());
    auto loaded_site = aetheria::serialize::decode_zone(site_bytes, test_ruleset());
    const auto& loaded_persistent =
        std::get<aetheria::zone::SitePayload>(loaded_site->payload).layers.persistent;
    ASSERT_TRUE(loaded_persistent.order.has_value());
    ASSERT_FALSE(loaded_persistent.named_npcs.empty());
    ASSERT_FALSE(loaded_persistent.dungeons.empty());
    EXPECT_EQ(loaded_persistent.order, persistent.order);
    EXPECT_TRUE(loaded_persistent.named_npcs.front().missing);
    EXPECT_TRUE(loaded_persistent.dungeons.front().cleared);
    EXPECT_EQ(loaded_persistent.dungeons.front().depth, saved_depth);

    aetheria::zone::Zone region{
        aetheria::zone::child_key(aetheria::zone::kRootZone, kReductionRegionId, 0)};
    std::get<aetheria::zone::RegionPayload>(region.payload).layers.emplace(0, std::move(tiles));
    const auto region_bytes = aetheria::serialize::encode_zone(region, test_ruleset());
    const auto loaded_region = aetheria::serialize::decode_zone(region_bytes, test_ruleset());
    const auto& loaded_tiles =
        std::get<aetheria::zone::RegionPayload>(loaded_region->payload).layers.at(0);
    EXPECT_EQ(loaded_tiles.reduction_value<aetheria::world::OrderReduction>(kReductionCoordinate),
              saved_order);
    std::cout << "world_observation_roundtrip order=" << saved_order
              << " missing=1 cleared=1 depth=" << saved_depth << '\n';
}

TEST(EmergentQuest, FoodDeliveryChangesTheObservedCityThroughOneReductionWrite) {
    auto tiles = reduction_region();
    auto live_site = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, test_ruleset());
    aetheria::site::enter_full_site(live_site, tiles, kReductionCoordinate);
    auto& economy = aetheria::site::city_build_state(live_site).economy;
    economy.food_stock = 40;
    aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, live_site, test_ruleset());

    const auto quests = aetheria::narrative::detect_emergent_quests(meaningful_needs());
    const auto delivery = std::ranges::find(quests, EmergentQuestKind::FoodDelivery,
                                            &aetheria::narrative::EmergentQuest::kind);
    ASSERT_NE(delivery, quests.end());
    const auto report =
        aetheria::narrative::complete_food_delivery(*delivery, tiles, live_site, test_ruleset());

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

}  // namespace

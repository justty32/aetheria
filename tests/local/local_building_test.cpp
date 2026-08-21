#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <type_traits>

#include "core/local/local_buildings.h"
#include "tests/local/local_test_support.h"
#include "tests/support/performance.h"

namespace {

using aetheria::tests::building_site_layer;
using aetheria::tests::kLocalCenter;
using aetheria::tests::kLocalSiteSeed;
using aetheria::tests::test_ruleset;

static_assert(!std::is_invocable_v<decltype(&aetheria::local::build_building_local_skeleton),
                                   const aetheria::local::LocalFastVars&, std::uint64_t,
                                   const aetheria::rules::Ruleset&>);

[[nodiscard]] aetheria::local::LocalSlowVars sample_slow(
    aetheria::site::SiteZoning zoning = aetheria::site::SiteZoning::Residential) {
    const auto parent = building_site_layer(zoning);
    return aetheria::local::project_local_slow_vars(parent, kLocalCenter, kLocalSiteSeed,
                                                    *test_ruleset().find_feature("feature.none"),
                                                    test_ruleset());
}

[[nodiscard]] std::size_t ground_room_count(
    const aetheria::local::BuildingLocalSkeleton& skeleton) {
    return std::ranges::count_if(skeleton.rooms, [](const auto& room) { return room.z == 0; });
}

TEST(LocalBuilding, RouteAHasScaledHousesRoomsDoorsFurnitureAndVerticalLayers) {
    const auto slow = sample_slow();
    const auto seed =
        aetheria::local::derive_local_seed(kLocalSiteSeed, kLocalCenter.x, kLocalCenter.y);
    const auto first = aetheria::local::build_building_local_skeleton(slow, seed, test_ruleset());
    const auto second = aetheria::local::build_building_local_skeleton(slow, seed, test_ruleset());
    const auto changed =
        aetheria::local::build_building_local_skeleton(slow, seed + 1U, test_ruleset());
    const auto first_hash = aetheria::local::hash_building_local_skeleton(first);
    EXPECT_EQ(first_hash, aetheria::local::hash_building_local_skeleton(second));
    EXPECT_NE(first_hash, aetheria::local::hash_building_local_skeleton(changed));
    EXPECT_TRUE(aetheria::local::valid_building_invariants(first, test_ruleset()));
    EXPECT_EQ(first.houses.size(), 15U);
    EXPECT_EQ(ground_room_count(first), 60U);
    EXPECT_GE(first.houses.size(), 10U);
    EXPECT_LE(first.houses.size(), 20U);
    EXPECT_GE(ground_room_count(first), first.houses.size() * 2U);
    EXPECT_TRUE(std::ranges::all_of(first.rooms, [](const auto& room) {
        return room.footprint.width >= 5U && room.footprint.height >= 5U;
    }));
    EXPECT_TRUE(first.layers.contains(-1));
    EXPECT_TRUE(first.layers.contains(0));
    EXPECT_TRUE(first.layers.contains(1));
    EXPECT_TRUE(std::ranges::none_of(first.houses, [](const auto& house) {
        const auto& rect = house.footprint;
        return 31U >= rect.x && 31U < rect.x + rect.width && 31U >= rect.y &&
               31U < rect.y + rect.height;
    }));
    EXPECT_GT(first.door_count, 0U);
    EXPECT_GT(first.window_count, 0U);
    EXPECT_GT(first.furniture.size(), 0U);
    EXPECT_GT(first.resident_statistics, 0U);
    EXPECT_EQ(first.ambient_resident_count, 0U);
    EXPECT_EQ(first.entity_count(), first.furniture.size());
    EXPECT_GE(first.entity_count(), 20U);
    EXPECT_LT(first.entity_count(), 100U);
    EXPECT_GT(first.vertical_links.size(), 0U);

    std::cout << "local_route_a houses=" << first.houses.size()
              << " ground_rooms=" << ground_room_count(first)
              << " total_rooms=" << first.rooms.size() << " doors=" << first.door_count
              << " windows=" << first.window_count << " furniture=" << first.furniture.size()
              << " resident_statistics=" << first.resident_statistics
              << " ambient_entities=" << first.ambient_resident_count
              << " total_entities=" << first.entity_count() << " layers=" << first.layers.size()
              << " vertical_links=" << first.vertical_links.size()
              << " normalized_hash=" << first_hash << '\n';
}

TEST(LocalBuilding, SealedRoomNegativeControlIsRejected) {
    const auto slow = sample_slow();
    const auto seed =
        aetheria::local::derive_local_seed(kLocalSiteSeed, kLocalCenter.x, kLocalCenter.y);
    auto sealed = aetheria::local::build_building_local_skeleton(slow, seed, test_ruleset());
    const auto wall = test_ruleset().local_building_rules().wall_edge;
    std::size_t sealed_door_segments{};
    for (auto& [z, tiles] : sealed.layers) {
        static_cast<void>(z);
        for (auto& edge : tiles.edges) {
            const auto* definition = test_ruleset().edge(edge);
            if (definition != nullptr &&
                (definition->flags &
                 (aetheria::rules::kEdgeGateFlag | aetheria::rules::kEdgeOpenableFlag)) != 0) {
                edge = wall;
                ++sealed_door_segments;
            }
        }
    }
    EXPECT_GT(sealed_door_segments, 0U);
    EXPECT_FALSE(aetheria::local::valid_building_invariants(sealed, test_ruleset()));
    std::cout << "local_route_a_negative sealed_door_segments=" << sealed_door_segments
              << " invariant_rejected=1\n";
}

TEST(LocalBuilding, ResidentsStayStatisticalUntilTheirHouseIsEntered) {
    const auto slow = sample_slow();
    const auto seed =
        aetheria::local::derive_local_seed(kLocalSiteSeed, kLocalCenter.x, kLocalCenter.y);
    auto generated = aetheria::local::build_building_local_skeleton(slow, seed, test_ruleset());
    const auto before = generated.entity_count();
    ASSERT_EQ(generated.ambient_resident_count, 0U);
    const auto expected = generated.houses.front().resident_count;
    aetheria::local::materialize_ambient_residents(generated, 0, seed);
    EXPECT_EQ(generated.ambient_resident_count, expected);
    EXPECT_EQ(generated.entity_count(), before + expected);
    EXPECT_TRUE(aetheria::local::valid_building_invariants(generated, test_ruleset()));
    aetheria::local::materialize_ambient_residents(generated, 0, seed);
    EXPECT_EQ(generated.ambient_resident_count, expected);
    EXPECT_LT(generated.entity_count(), 100U);
    std::cout << "local_route_a_entry statistical_residents=" << generated.resident_statistics
              << " entered_house_residents=" << expected << " entities_before=" << before
              << " entities_after=" << generated.entity_count() << '\n';
}

TEST(LocalBuilding, RouteAIsUnderTenMillisecondBudgetWithRealOutput) {
    const auto slow = sample_slow(aetheria::site::SiteZoning::Commercial);
    const auto seed =
        aetheria::local::derive_local_seed(kLocalSiteSeed, kLocalCenter.x, kLocalCenter.y);
    const auto witness = aetheria::local::build_building_local_skeleton(slow, seed, test_ruleset());
    const auto minimum_milliseconds = aetheria::tests::minimum_milliseconds_after_warmup([&] {
        const auto start = std::chrono::steady_clock::now();
        const auto generated =
            aetheria::local::build_building_local_skeleton(slow, seed, test_ruleset());
        EXPECT_GT(generated.houses.size(), 0U);
        EXPECT_GT(generated.rooms.size(), 0U);
        EXPECT_GT(generated.door_count, 0U);
        EXPECT_GT(generated.furniture.size(), 0U);
        return std::chrono::duration<double, std::milli>{std::chrono::steady_clock::now() - start}
            .count();
    });
    std::cout << "local_route_a_min_of_5_ms=" << minimum_milliseconds
              << " houses=" << witness.houses.size() << " rooms=" << witness.rooms.size()
              << " doors=" << witness.door_count << " furniture=" << witness.furniture.size()
              << " entities=" << witness.entity_count() << '\n';
    EXPECT_LT(minimum_milliseconds, 10.0);
}

}  // namespace

#include "tests/site/site_roundtrip_test_support.h"

#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::site::BuildingState;
using aetheria::tests::building_state;
using aetheria::tests::disk_world_hash;
using aetheria::tests::kRoundTripCoordinate;
using aetheria::tests::kRoundTripRegionId;
using aetheria::tests::kRoundTripSiteKey;
using aetheria::tests::kRoundTripWorldSeed;
using aetheria::tests::prepare_idle_digest;
using aetheria::tests::round_trip_region;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::zone::FileZoneStore;
using aetheria::zone::ZoneManager;

TEST(SiteRoundTrip, PersistentMutationBetweenCollapseAndExpandBreaksHashSequence) {
    TemporaryDirectory directory;
    const auto tiles = round_trip_region();
    FileZoneStore store{directory.path(), test_ruleset()};
    static_cast<void>(prepare_idle_digest(store, tiles));
    ZoneManager manager{store};

    ASSERT_FALSE(manager.get(kRoundTripSiteKey).has_value());
    const auto handle = aetheria::site::rematerialize_site_zone(
        manager, tiles, kRoundTripCoordinate, kRoundTripWorldSeed, kRoundTripRegionId,
        test_ruleset());
    aetheria::site::collapse_site_zone(manager, handle);
    ASSERT_FALSE(manager.get(kRoundTripSiteKey).has_value());
    const auto collapsed_hash = disk_world_hash(directory);

    auto changed = store.load(kRoundTripSiteKey);
    ASSERT_NE(changed, nullptr);
    auto& buildings =
        std::get<aetheria::zone::SitePayload>(changed->payload).layers.persistent.buildings;
    ASSERT_EQ(buildings.size(), 1U);
    ASSERT_EQ(buildings.front().state, BuildingState::Idle);
    buildings.front().state = BuildingState::Derelict;
    store.save(*changed);
    const auto changed_hash = disk_world_hash(directory);

    ASSERT_FALSE(manager.get(kRoundTripSiteKey).has_value());
    const auto changed_handle = aetheria::site::rematerialize_site_zone(
        manager, tiles, kRoundTripCoordinate, kRoundTripWorldSeed, kRoundTripRegionId,
        test_ruleset());
    manager.save_all();
    const auto expanded_hash = disk_world_hash(directory);
    EXPECT_EQ(building_state(manager, changed_handle), BuildingState::Derelict);
    std::cout << "site_roundtrip_negative collapsed=" << collapsed_hash
              << " mutated=" << changed_hash << " expanded=" << expanded_hash
              << " state=" << static_cast<unsigned>(BuildingState::Derelict)
              << " (Derelict)\n";
    EXPECT_NE(collapsed_hash, changed_hash);
    EXPECT_EQ(changed_hash, expanded_hash);
}

}  // namespace

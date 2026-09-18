#include "tests/site/site_roundtrip_test_support.h"
#include "tests/support/performance.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

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
using aetheria::zone::Zone;
using aetheria::zone::ZoneManager;

TEST(SiteRoundTrip, ZoneManagerAcquireRunsThreeStepRematerializeAndKeepsBuildingTile) {
    TemporaryDirectory directory;
    auto tiles = round_trip_region();
    FileZoneStore store{directory.path(), test_ruleset()};
    static_cast<void>(prepare_idle_digest(store, tiles));
    const auto disk_only = store.load(kRoundTripSiteKey);
    ASSERT_NE(disk_only, nullptr);
    const auto& disk_layers =
        std::get<aetheria::zone::SitePayload>(disk_only->payload).layers;
    ASSERT_TRUE(disk_layers.procedural.skeleton.ground.empty());
    ASSERT_EQ(disk_layers.persistent.buildings.size(), 1U);
    const auto saved_tile = disk_layers.persistent.buildings.front().tile;

    ZoneManager manager{store};
    const auto acquired = manager.acquire(
        kRoundTripSiteKey,
        [&](aetheria::zone::ZoneKey key, std::unique_ptr<Zone> persistent) {
            if (key != kRoundTripSiteKey || persistent == nullptr) {
                return std::unique_ptr<Zone>{};
            }
            auto site = aetheria::site::rematerialize_site_zone(
                std::move(*persistent), tiles, kRoundTripCoordinate, kRoundTripWorldSeed,
                kRoundTripRegionId, aetheria::time::Tick{}, test_ruleset());
            return std::make_unique<Zone>(std::move(site));
        });
    ASSERT_TRUE(acquired.has_value());
    ASSERT_TRUE(manager.with(*acquired, [&](const Zone& site) {
        const auto& layers = std::get<aetheria::zone::SitePayload>(site.payload).layers;
        EXPECT_EQ(layers.procedural.skeleton.ground.size(), aetheria::site::kSiteTileCount);
        ASSERT_EQ(layers.persistent.buildings.size(), 1U);
        EXPECT_EQ(layers.persistent.buildings.front().tile, saved_tile);
        EXPECT_EQ(layers.persistent.buildings.front().state, BuildingState::Idle);
        std::cout << "site_acquire raw_tiles=0 rematerialized_tiles="
                  << layers.procedural.skeleton.ground.size() << " persistent_objects=1 at="
                  << saved_tile.x << ',' << saved_tile.y << '\n';
    }));
}

}  // namespace

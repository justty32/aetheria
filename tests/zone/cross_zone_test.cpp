#include <aetheria/runtime/cross_zone.h>

#include "core/local/local_materialize.h"
#include "core/serialize/normalized_state_hash.h"
#include "core/world/region_movement.h"
#include "core/zone/zone.h"
#include "core/zone/zone_manager.h"
#include "tests/local/local_test_support.h"
#include "tests/support/ruleset_fixture.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>

#include <gtest/gtest.h>

#include "tests/zone/cross_zone_test_support.h"

namespace {

using aetheria::local::LocalXY;
using aetheria::runtime::CrossZoneRuntime;
using aetheria::runtime::EntityRef;
using aetheria::runtime::LocalPosition;
using aetheria::tests::open_site_layer;
using aetheria::tests::test_ruleset;
using aetheria::world::MovementPoints;
using aetheria::world::StableId;
using aetheria::zone::InMemoryZoneStore;
using aetheria::zone::Zone;
using aetheria::zone::ZoneKey;
using aetheria::zone::ZoneManager;

using namespace aetheria::tests::cross_zone;



TEST(ZoneManagerAcquire, ColdLoadRestores4096TilesAndPreservesPersistentObject) {
    const auto parent = open_site_layer();
    const auto feature = *test_ruleset().find_feature("feature.none");
    auto saved = aetheria::local::materialize_local_zone(
        kSite, parent, {12, 12}, aetheria::tests::kLocalSiteSeed, feature,
        test_ruleset());
    static_cast<void>(add_persistent_object(saved, 6001));
    InMemoryZoneStore store{test_ruleset()};
    store.save(saved);

    ZoneManager manager{store, local_materializer(parent)};
    ASSERT_TRUE(manager.load(kDestination));
    std::size_t raw_tile_count = 1;
    std::size_t raw_persistent_count{};
    ASSERT_TRUE(manager.with(*manager.get(kDestination), [&](const Zone& loaded) {
        const auto& layers = std::get<aetheria::zone::LocalPayload>(loaded.payload).layers;
        raw_tile_count = layers.contains(0) ? layers.at(0).ground.size() : 0U;
        raw_persistent_count = loaded.reg.view<const StableId>().size();
    }));
    EXPECT_EQ(raw_tile_count, 0U);
    EXPECT_EQ(raw_persistent_count, 1U);

    const auto acquired = manager.acquire(kDestination);
    ASSERT_TRUE(acquired.has_value());
    std::size_t tile_count{};
    std::size_t persistent_count{};
    ASSERT_TRUE(manager.with(*acquired, [&](const Zone& loaded) {
        const auto& layers = std::get<aetheria::zone::LocalPayload>(loaded.payload).layers;
        tile_count = layers.contains(0) ? layers.at(0).ground.size() : 0U;
        persistent_count = loaded.reg.view<const StableId>().size();
    }));
    EXPECT_EQ(tile_count, aetheria::local::kLocalTileCount);
    EXPECT_EQ(persistent_count, 1U);
    std::cout << "zone_acquire raw_tiles=" << raw_tile_count
              << " rematerialized_tiles=" << tile_count
              << " persistent_objects=" << persistent_count << '\n';
}

TEST(ZoneManagerAcquire, MissingSnapshotGeneratesAndInvalidResultLeavesNoShell) {
    const auto parent = open_site_layer();
    InMemoryZoneStore store{test_ruleset()};
    ZoneManager manager{store, local_materializer(parent)};

    const auto generated = manager.acquire(kDestination);
    ASSERT_TRUE(generated.has_value());
    std::size_t tile_count{};
    ASSERT_TRUE(manager.with(*generated, [&](const Zone& loaded) {
        tile_count = std::get<aetheria::zone::LocalPayload>(loaded.payload)
                         .layers.at(0)
                         .ground.size();
    }));
    EXPECT_EQ(tile_count, aetheria::local::kLocalTileCount);

    const auto failed_key = aetheria::zone::child_key(kSite, 13, 12);
    ZoneManager failing_manager{
        store, [](ZoneKey key, std::unique_ptr<Zone>) { return std::make_unique<Zone>(key); }};
    const bool failure_returned_handle = failing_manager.acquire(failed_key).has_value();
    const bool failure_left_shell = failing_manager.get(failed_key).has_value();
    EXPECT_FALSE(failure_returned_handle);
    EXPECT_FALSE(failure_left_shell);
    std::cout << "zone_acquire generated_tiles=" << tile_count
              << " invalid_returned_handle=" << failure_returned_handle
              << " invalid_left_shell=" << failure_left_shell << '\n';
}

TEST(CrossZonePeek, LoadedLocalReturnsValueAndAbsentIsExpectedUnknown) {
    InMemoryZoneStore store{test_ruleset()};
    ZoneManager manager{store};
    static_cast<void>(manager.adopt(local_zone(kSource, 9)));
    CrossZoneRuntime runtime{manager};

    const auto tile = runtime.peek_tile(kSource, {63, 63});
    ASSERT_TRUE(tile.has_value());
    EXPECT_EQ(tile->ground, aetheria::rules::GroundId{9});
    EXPECT_EQ(tile->overlay, aetheria::local::OverlayId::Vegetation);
    EXPECT_EQ(tile->occupant, 9U);
    EXPECT_EQ(tile->light, 9U);
    const auto edge = runtime.peek_edge(kSource, {63, 63},
                                        aetheria::spatial::BoundarySide::East);
    ASSERT_TRUE(edge.has_value());
    EXPECT_EQ(edge->edge, aetheria::rules::EdgeId{9});

    EXPECT_NO_THROW({
        EXPECT_FALSE(runtime.peek_tile(kDestination, {0, 0}).has_value());
        EXPECT_FALSE(runtime.peek_edge(kDestination, {0, 0},
                                       aetheria::spatial::BoundarySide::West)
                         .has_value());
    });
    EXPECT_FALSE(runtime.peek_tile(kSource, {64, 0}).has_value());
}

}  // namespace

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



TEST(CrossZoneMigration, MissingDestinationReturnsFalseWithoutChangingSource) {
    InMemoryZoneStore store{test_ruleset()};
    auto source = local_zone(kSource);
    const auto old_handle = add_actor(*source, 88, {3, 4});
    ZoneManager manager{store};
    static_cast<void>(manager.adopt(std::move(source)));
    CrossZoneRuntime runtime{manager};

    EXPECT_FALSE(runtime.migrate_entity(kSource, old_handle, kDestination, {5, 6}));
    EXPECT_FALSE(manager.get(kDestination).has_value());
    EXPECT_EQ(runtime.resolve(EntityRef{kSource, 88}), old_handle);
    ASSERT_TRUE(manager.with(*manager.get(kSource), [&](const Zone& loaded) {
        EXPECT_TRUE(loaded.reg.valid(old_handle));
        EXPECT_EQ(loaded.reg.get<const MovementPoints>(old_handle), (MovementPoints{7, 13}));
        EXPECT_EQ(loaded.reg.get<const LocalPosition>(old_handle).tile, (LocalXY{3, 4}));
        EXPECT_EQ(loaded.uid_index.at(88), old_handle);
    }));
}

TEST(CrossZoneMigration, MissingDestinationDuringTickReturnsFalseWithoutStructuralLoad) {
    InMemoryZoneStore store{test_ruleset()};
    auto source = local_zone(kSource);
    const auto old_handle = add_actor(*source, 89, {3, 4});
    ZoneManager manager{store};
    static_cast<void>(manager.adopt(std::move(source)));
    CrossZoneRuntime runtime{manager};

    manager.tick(aetheria::time::Tick{1}, [&](Zone& loaded) {
        if (loaded.key == kSource) {
            EXPECT_FALSE(runtime.migrate_entity(kSource, old_handle, kDestination, {5, 6}));
            EXPECT_TRUE(loaded.reg.valid(old_handle));
        }
    });
    EXPECT_FALSE(manager.get(kDestination).has_value());
    EXPECT_EQ(runtime.resolve({kSource, 89}), old_handle);
}

TEST(CrossZoneMigration, MidTransactionUidConflictRollsBackStagedDestination) {
    InMemoryZoneStore store{test_ruleset()};
    auto source = local_zone(kSource);
    const auto old_handle = add_actor(*source, 99, {7, 8});
    auto destination = local_zone(kDestination);
    const auto incumbent = add_actor(*destination, 99, {9, 10});
    ZoneManager manager{store};
    static_cast<void>(manager.adopt(std::move(source)));
    static_cast<void>(manager.adopt(std::move(destination)));
    CrossZoneRuntime runtime{manager};

    EXPECT_FALSE(runtime.migrate_entity(kSource, old_handle, kDestination, {11, 12}));
    EXPECT_EQ(runtime.resolve({kSource, 99}), old_handle);
    EXPECT_EQ(runtime.resolve({kDestination, 99}), incumbent);
    ASSERT_TRUE(manager.with(*manager.get(kSource), [&](const Zone& loaded) {
        EXPECT_TRUE(loaded.reg.valid(old_handle));
        EXPECT_EQ(loaded.reg.view<const StableId>().size(), 1U);
    }));
    ASSERT_TRUE(manager.with(*manager.get(kDestination), [&](const Zone& loaded) {
        EXPECT_TRUE(loaded.reg.valid(incumbent));
        EXPECT_EQ(loaded.reg.view<const StableId>().size(), 1U);
        EXPECT_EQ(loaded.touch_count, 0U);
    }));
}

}  // namespace

#include "core/time/tick.h"
#include "core/zone/zone_key.h"
#include "core/zone/zone_manager.h"
#include "tests/support/ruleset_fixture.h"

#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::time::Tick;
using aetheria::tests::test_ruleset;
using aetheria::zone::child_key;
using aetheria::zone::detached_id_of;
using aetheria::zone::DetachedZoneKeyAllocator;
using aetheria::zone::InMemoryZoneStore;
using aetheria::zone::kRootZone;
using aetheria::zone::Zone;
using aetheria::zone::ZoneKey;
using aetheria::zone::ZoneManager;

TEST(ZoneManager, RootCannotBeUnloadedOrDestroyed) {
    InMemoryZoneStore store{test_ruleset()};
    ZoneManager manager{store};

    EXPECT_TRUE(manager.get(kRootZone).has_value());
    EXPECT_FALSE(manager.unload(kRootZone));
    EXPECT_FALSE(manager.destroy(kRootZone));
    EXPECT_TRUE(manager.get(kRootZone).has_value());
}

TEST(ZoneManager, RootDoesNotParticipateInTick) {
    InMemoryZoneStore store{test_ruleset()};
    ZoneManager manager{store};
    std::vector<ZoneKey> visited;

    manager.tick(Tick{1}, [&](Zone& zone) { visited.push_back(zone.key); });
    EXPECT_TRUE(visited.empty());

    const auto region = child_key(kRootZone, 1, 0);
    static_cast<void>(manager.materialize(region));
    manager.tick(Tick{2}, [&](Zone& zone) { visited.push_back(zone.key); });
    EXPECT_EQ(visited, (std::vector<ZoneKey>{region}));
}

TEST(ZoneManager, DetachedIdsAreNeverReusedAfterDestroy) {
    InMemoryZoneStore store{test_ruleset()};
    ZoneManager manager{store};
    DetachedZoneKeyAllocator allocator;

    const auto first = allocator.allocate();
    static_cast<void>(manager.materialize(first));
    ASSERT_TRUE(manager.destroy(first));
    const auto second = allocator.allocate();

    EXPECT_NE(first, second);
    EXPECT_EQ(detached_id_of(second), detached_id_of(first) + 1);
}

TEST(ZoneManagerDeathTest, RejectsStructuralChangesDuringTick) {
    InMemoryZoneStore store{test_ruleset()};
    ZoneManager manager{store};
    const auto loaded = child_key(kRootZone, 1, 0);
    const auto absent = child_key(kRootZone, 2, 0);
    static_cast<void>(manager.materialize(loaded));

    manager.tick(Tick{1}, [&](Zone&) {
        EXPECT_DEATH(static_cast<void>(manager.materialize(absent)),
                     "AETH_CHECK failed: !in_tick_");
        EXPECT_DEATH(static_cast<void>(manager.load(absent)), "AETH_CHECK failed: !in_tick_");
        EXPECT_DEATH(static_cast<void>(manager.unload(loaded)), "AETH_CHECK failed: !in_tick_");
        EXPECT_DEATH(static_cast<void>(manager.destroy(loaded)), "AETH_CHECK failed: !in_tick_");
    });
}

TEST(ZoneManager, AppliesQueuedStructuralChangesAtTickEndInFifoOrder) {
    InMemoryZoneStore store{test_ruleset()};
    ZoneManager manager{store};
    const auto first = child_key(kRootZone, 1, 0);
    const auto second = child_key(kRootZone, 2, 0);
    static_cast<void>(manager.materialize(first));

    manager.tick(Tick{10}, [&](Zone&) {
        manager.queue_materialize(second);
        manager.queue_unload(second);
        manager.queue_materialize(second);
        manager.queue_destroy(first);
    });

    EXPECT_FALSE(manager.get(first).has_value());
    EXPECT_TRUE(manager.get(second).has_value());
    EXPECT_EQ(manager.tick_order(), (std::vector<ZoneKey>{second}));
}

TEST(ZoneManager, SavesDifferentZonesAtTheirOwnTicks) {
    InMemoryZoneStore store{test_ruleset()};
    ZoneManager manager{store};
    const auto first = child_key(kRootZone, 1, 0);
    const auto second = child_key(kRootZone, 2, 0);
    static_cast<void>(manager.materialize(first));
    static_cast<void>(manager.materialize(second));

    manager.tick(Tick{100}, [&](Zone& zone) {
        if (zone.key == first) {
            manager.queue_unload(first);
        }
    });
    manager.tick(Tick{250}, [&](Zone& zone) {
        if (zone.key == second) {
            manager.queue_unload(second);
        }
    });

    const auto saved_first = store.load(first);
    const auto saved_second = store.load(second);
    ASSERT_NE(saved_first, nullptr);
    ASSERT_NE(saved_second, nullptr);
    EXPECT_EQ(saved_first->last_saved_tick, Tick{100});
    EXPECT_EQ(saved_second->last_saved_tick, Tick{250});
}

TEST(ZoneManager, RequireAndProbeLoadTreatMissingZoneDifferently) {
    InMemoryZoneStore store{test_ruleset()};
    ZoneManager manager{store};
    const auto missing = child_key(kRootZone, 9, 0);

    EXPECT_FALSE(manager.load(missing));
    EXPECT_THROW(static_cast<void>(manager.require(missing)), std::runtime_error);
}

}  // namespace

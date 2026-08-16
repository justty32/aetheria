#include "core/time/tick.h"
#include "core/zone/zone_key.h"
#include "core/zone/zone_manager.h"
#include "tests/support/ruleset_fixture.h"

#include <concepts>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::time::Tick;
using aetheria::zone::child_key;
using aetheria::zone::detached_id_of;
using aetheria::zone::DetachedZoneKeyAllocator;
using aetheria::zone::InMemoryZoneStore;
using aetheria::zone::kRootZone;
using aetheria::zone::level_of;
using aetheria::zone::level_value_of;
using aetheria::zone::local_x_of;
using aetheria::zone::local_y_of;
using aetheria::zone::parent_of;
using aetheria::zone::region_id_of;
using aetheria::zone::site_x_of;
using aetheria::zone::site_y_of;
using aetheria::zone::Zone;
using aetheria::zone::ZoneHandle;
using aetheria::zone::ZoneKey;
using aetheria::zone::ZoneLevel;
using aetheria::zone::ZoneManager;
using aetheria::tests::test_ruleset;

constexpr auto kRegion = child_key(kRootZone, UINT16_C(0xA55A), 0);
constexpr auto kSite = child_key(kRegion, UINT16_C(0x0ABC), UINT16_C(0x0123));
constexpr auto kLocal = child_key(kSite, UINT16_C(0x0321), UINT16_C(0x0012));

static_assert(level_of(kRootZone) == ZoneLevel::Root);
static_assert(level_of(kRegion) == ZoneLevel::Region);
static_assert(level_of(kSite) == ZoneLevel::Site);
static_assert(level_of(kLocal) == ZoneLevel::Local);
static_assert(region_id_of(kRegion) == UINT16_C(0xA55A));
static_assert(site_x_of(kSite) == UINT16_C(0x0ABC));
static_assert(site_y_of(kSite) == UINT16_C(0x0123));
static_assert(local_x_of(kLocal) == UINT16_C(0x0321));
static_assert(local_y_of(kLocal) == UINT16_C(0x0012));
static_assert(parent_of(kRegion) == kRootZone);
static_assert(parent_of(kSite) == kRegion);
static_assert(parent_of(kLocal) == kSite);
static_assert(!std::is_copy_constructible_v<Zone>);
static_assert(std::is_move_constructible_v<Zone>);
static_assert(std::is_copy_constructible_v<ZoneHandle>);
static_assert(
    std::same_as<decltype(std::declval<ZoneManager&>().get(kRootZone)), std::optional<ZoneHandle>>);
static_assert(!std::convertible_to<decltype(std::declval<ZoneManager&>().get(kRootZone)),
                                   aetheria::zone::Zone*>);

TEST(ZoneKey, RoundTripsEveryCoordinateValueAtEachLevel) {
    for (std::uint32_t region_id = 0; region_id <= UINT16_MAX; ++region_id) {
        const auto region = child_key(kRootZone, region_id, 0);
        ASSERT_EQ(parent_of(region), kRootZone);
        ASSERT_EQ(region_id_of(region), region_id);
    }

    constexpr auto region = child_key(kRootZone, UINT16_MAX, 0);
    for (std::uint32_t x = 0; x <= UINT16_C(0x0FFF); ++x) {
        for (std::uint32_t y = 0; y <= UINT16_C(0x0FFF); ++y) {
            const auto site = child_key(region, x, y);
            ASSERT_EQ(parent_of(site), region);
            ASSERT_EQ(local_x_of(site), x);
            ASSERT_EQ(local_y_of(site), y);
        }
    }

    constexpr auto site = child_key(region, UINT16_C(0x0FFF), UINT16_C(0x0FFF));
    for (std::uint32_t x = 0; x <= UINT16_C(0x03FF); ++x) {
        for (std::uint32_t y = 0; y <= UINT16_C(0x03FF); ++y) {
            const auto local = child_key(site, x, y);
            ASSERT_EQ(parent_of(local), site);
            ASSERT_EQ(local_x_of(local), x);
            ASSERT_EQ(local_y_of(local), y);
        }
    }
}

TEST(ZoneKey, CoversEveryDefinedLevelAndFieldBoundary) {
    DetachedZoneKeyAllocator allocator;
    const auto detached = allocator.allocate();

    EXPECT_EQ(level_value_of(kRootZone), 0);
    EXPECT_EQ(level_value_of(child_key(kRootZone, 0, 0)), 1);
    EXPECT_EQ(level_value_of(child_key(child_key(kRootZone, 0, 0), 0, 0)), 2);
    EXPECT_EQ(level_value_of(child_key(child_key(child_key(kRootZone, 0, 0), 0, 0), 0, 0)), 3);
    EXPECT_EQ(level_value_of(detached), 15);
    EXPECT_EQ(detached_id_of(detached), 1);

    const auto max_region = child_key(kRootZone, UINT16_MAX, 0);
    const auto max_site = child_key(max_region, UINT16_C(0x0FFF), UINT16_C(0x0FFF));
    const auto max_local = child_key(max_site, UINT16_C(0x03FF), UINT16_C(0x03FF));
    EXPECT_EQ(region_id_of(max_local), UINT16_MAX);
    EXPECT_EQ(site_x_of(max_local), UINT16_C(0x0FFF));
    EXPECT_EQ(site_y_of(max_local), UINT16_C(0x0FFF));
    EXPECT_EQ(local_x_of(max_local), UINT16_C(0x03FF));
    EXPECT_EQ(local_y_of(max_local), UINT16_C(0x03FF));

    DetachedZoneKeyAllocator maximum_allocator{UINT64_C(0x0FFFFFFFFFFFFFFF)};
    const auto maximum_detached = maximum_allocator.allocate();
    EXPECT_EQ(detached_id_of(maximum_detached), UINT64_C(0x0FFFFFFFFFFFFFFF));
    EXPECT_DEATH(static_cast<void>(maximum_allocator.allocate()), "AETH_CHECK failed: next_id_");
    EXPECT_DEATH(DetachedZoneKeyAllocator{0}, "AETH_CHECK failed: next_id_");
}

TEST(ZoneKey, RootAndDetachedHaveRootParent) {
    DetachedZoneKeyAllocator allocator;
    EXPECT_EQ(parent_of(kRootZone), kRootZone);
    EXPECT_EQ(parent_of(allocator.allocate()), kRootZone);
}

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

TEST(ZoneManager, UnloadAndLoadTransferTheSameSubstantiveZone) {
    InMemoryZoneStore store{test_ruleset()};
    ZoneManager manager{store};
    const auto key = child_key(kRootZone, 12, 0);
    const auto handle = manager.materialize(key);
    ASSERT_TRUE(manager.with(handle, [](Zone& zone) {
        zone.region_tiles.emplace(1, 1);
        zone.region_tiles->temperature.at(0) = 73;
    }));

    ASSERT_TRUE(manager.unload(key));
    EXPECT_FALSE(manager.get(key).has_value());
    EXPECT_FALSE(manager.with(handle, [](Zone&) {}));
    EXPECT_TRUE(store.contains(key));
    ASSERT_TRUE(manager.load(key));
    std::uint16_t loaded_tile{};
    ASSERT_TRUE(manager.with(
        handle, [&](const Zone& zone) { loaded_tile = zone.region_tiles->temperature.at(0); }));
    EXPECT_EQ(loaded_tile, 73);
    EXPECT_TRUE(store.contains(key));
}

TEST(ZoneManager, TickBorrowWritesZoneDataVisibleAfterTheTurn) {
    InMemoryZoneStore store{test_ruleset()};
    ZoneManager manager{store};
    const auto handle = manager.materialize(child_key(kRootZone, 13, 0));

    manager.tick(Tick{10}, [](Zone& zone) {
        zone.region_tiles.emplace(1, 1);
        zone.region_tiles->temperature.at(0) = 91;
    });

    std::uint16_t observed{};
    ASSERT_TRUE(manager.with(
        handle, [&](const Zone& zone) { observed = zone.region_tiles->temperature.at(0); }));
    EXPECT_EQ(observed, 91);
}

TEST(ZoneManager, QueuedUnloadKeepsTickBorrowAliveUntilCallbackReturns) {
    InMemoryZoneStore store{test_ruleset()};
    ZoneManager manager{store};
    const auto handle = manager.materialize(child_key(kRootZone, 14, 0));

    manager.tick(Tick{20}, [&](Zone& zone) {
        zone.region_tiles.emplace(1, 1);
        zone.region_tiles->temperature.at(0) = 101;
        manager.queue_unload(zone.key);
        zone.region_tiles->temperature.at(0) = 102;
    });

    EXPECT_FALSE(manager.with(handle, [](Zone&) {}));
    ASSERT_TRUE(manager.load(handle.key()));
    std::uint16_t observed{};
    ASSERT_TRUE(manager.with(
        handle, [&](const Zone& zone) { observed = zone.region_tiles->temperature.at(0); }));
    EXPECT_EQ(observed, 102);
}

struct ScenarioResult {
    std::vector<ZoneKey> loaded;
    std::vector<std::vector<ZoneKey>> tick_visits;

    bool operator==(const ScenarioResult&) const = default;
};

ScenarioResult run_deterministic_scenario() {
    InMemoryZoneStore store{test_ruleset()};
    ZoneManager manager{store};
    const auto region_a = child_key(kRootZone, 7, 0);
    const auto region_b = child_key(kRootZone, 3, 0);
    const auto site = child_key(region_a, 4, 5);
    static_cast<void>(manager.materialize(region_a));
    static_cast<void>(manager.materialize(region_b));

    ScenarioResult result;
    result.tick_visits.emplace_back();
    manager.tick(Tick{10}, [&](Zone& zone) {
        result.tick_visits.back().push_back(zone.key);
        if (zone.key == region_a) {
            manager.queue_materialize(site);
            manager.queue_unload(region_b);
        }
    });
    result.tick_visits.emplace_back();
    manager.tick(Tick{20}, [&](Zone& zone) {
        result.tick_visits.back().push_back(zone.key);
        if (zone.key == site) {
            manager.queue_materialize(region_b);
        }
    });
    result.loaded = manager.loaded_keys();
    return result;
}

TEST(ZoneManager, ReplaysCommandsWithIdenticalLoadedSetAndTickOrder) {
    EXPECT_EQ(run_deterministic_scenario(), run_deterministic_scenario());
}

}  // namespace

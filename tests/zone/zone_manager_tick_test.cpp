#include "core/time/tick.h"
#include "core/zone/zone_key.h"
#include "core/zone/zone_manager.h"
#include "tests/support/ruleset_fixture.h"

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::time::Tick;
using aetheria::tests::test_ruleset;
using aetheria::zone::child_key;
using aetheria::zone::InMemoryZoneStore;
using aetheria::zone::kRootZone;
using aetheria::zone::Zone;
using aetheria::zone::ZoneKey;
using aetheria::zone::ZoneManager;

TEST(ZoneManager, UnloadAndLoadTransferTheSameSubstantiveZone) {
    InMemoryZoneStore store{test_ruleset()};
    ZoneManager manager{store};
    const auto key = child_key(kRootZone, 12, 0);
    const auto handle = manager.materialize(key);
    ASSERT_TRUE(manager.with(handle, [](Zone& zone) {
        auto& layers = std::get<aetheria::zone::RegionPayload>(zone.payload).layers;
        layers.emplace(0, aetheria::world::RegionTiles{1, 1}).first->second.temperature.at(0) = 73;
    }));

    ASSERT_TRUE(manager.unload(key));
    EXPECT_FALSE(manager.get(key).has_value());
    EXPECT_FALSE(manager.with(handle, [](Zone&) {}));
    EXPECT_TRUE(store.contains(key));
    ASSERT_TRUE(manager.load(key));
    std::uint16_t loaded_tile{};
    ASSERT_TRUE(manager.with(handle, [&](const Zone& zone) {
        loaded_tile =
            std::get<aetheria::zone::RegionPayload>(zone.payload).layers.at(0).temperature.at(0);
    }));
    EXPECT_EQ(loaded_tile, 73);
    EXPECT_TRUE(store.contains(key));
}

TEST(ZoneManager, TickBorrowWritesZoneDataVisibleAfterTheTurn) {
    InMemoryZoneStore store{test_ruleset()};
    ZoneManager manager{store};
    const auto handle = manager.materialize(child_key(kRootZone, 13, 0));

    manager.tick(Tick{10}, [](Zone& zone) {
        auto& layers = std::get<aetheria::zone::RegionPayload>(zone.payload).layers;
        layers.emplace(0, aetheria::world::RegionTiles{1, 1}).first->second.temperature.at(0) = 91;
    });

    std::uint16_t observed{};
    ASSERT_TRUE(manager.with(handle, [&](const Zone& zone) {
        observed =
            std::get<aetheria::zone::RegionPayload>(zone.payload).layers.at(0).temperature.at(0);
    }));
    EXPECT_EQ(observed, 91);
}

TEST(ZoneManager, QueuedUnloadKeepsTickBorrowAliveUntilCallbackReturns) {
    InMemoryZoneStore store{test_ruleset()};
    ZoneManager manager{store};
    const auto handle = manager.materialize(child_key(kRootZone, 14, 0));

    manager.tick(Tick{20}, [&](Zone& zone) {
        auto& layers = std::get<aetheria::zone::RegionPayload>(zone.payload).layers;
        auto& tiles = layers.emplace(0, aetheria::world::RegionTiles{1, 1}).first->second;
        tiles.temperature.at(0) = 101;
        manager.queue_unload(zone.key);
        tiles.temperature.at(0) = 102;
    });

    EXPECT_FALSE(manager.with(handle, [](Zone&) {}));
    ASSERT_TRUE(manager.load(handle.key()));
    std::uint16_t observed{};
    ASSERT_TRUE(manager.with(handle, [&](const Zone& zone) {
        observed =
            std::get<aetheria::zone::RegionPayload>(zone.payload).layers.at(0).temperature.at(0);
    }));
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

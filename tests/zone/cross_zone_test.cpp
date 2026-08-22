#include <aetheria/runtime/cross_zone.h>

#include "core/serialize/normalized_state_hash.h"
#include "core/world/region_movement.h"
#include "core/zone/zone.h"
#include "core/zone/zone_manager.h"
#include "tests/support/ruleset_fixture.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>

#include <gtest/gtest.h>

namespace {

using aetheria::local::LocalXY;
using aetheria::runtime::CrossZoneRuntime;
using aetheria::runtime::EntityRef;
using aetheria::runtime::LocalPosition;
using aetheria::tests::test_ruleset;
using aetheria::world::MovementPoints;
using aetheria::world::StableId;
using aetheria::zone::InMemoryZoneStore;
using aetheria::zone::Zone;
using aetheria::zone::ZoneKey;
using aetheria::zone::ZoneManager;

constexpr auto kRegion = aetheria::zone::child_key(aetheria::zone::kRootZone, 7, 0);
constexpr auto kSite = aetheria::zone::child_key(kRegion, 4, 5);
constexpr auto kSource = aetheria::zone::child_key(kSite, 11, 12);
constexpr auto kDestination = aetheria::zone::child_key(kSite, 12, 12);

[[nodiscard]] std::unique_ptr<Zone> local_zone(ZoneKey key, std::uint16_t marker = 0) {
    auto result = std::make_unique<Zone>(key);
    aetheria::local::LocalTiles tiles;
    tiles.ground.assign(aetheria::local::kLocalTileCount,
                        aetheria::rules::GroundId{marker});
    tiles.overlay.assign(aetheria::local::kLocalTileCount,
                         aetheria::local::OverlayId::Vegetation);
    tiles.occupant.assign(aetheria::local::kLocalTileCount, marker);
    tiles.edges.assign(aetheria::local::kLocalTileCount * 4U,
                       aetheria::rules::EdgeId{marker});
    tiles.light.assign(aetheria::local::kLocalTileCount,
                       static_cast<std::uint8_t>(marker));
    std::get<aetheria::zone::LocalPayload>(result->payload).layers.emplace(0,
                                                                          std::move(tiles));
    return result;
}

class PreparedLocalStore final : public aetheria::zone::ZoneStore {
public:
    PreparedLocalStore(ZoneKey prepared, std::uint16_t marker)
        : prepared_{prepared}, marker_{marker} {}

    [[nodiscard]] bool contains(ZoneKey key) const override { return key == prepared_; }
    [[nodiscard]] std::unique_ptr<Zone> load(ZoneKey key) const override {
        return key == prepared_ ? local_zone(key, marker_) : nullptr;
    }
    void save(const Zone&) override {}
    [[nodiscard]] bool erase(ZoneKey) override { return false; }

private:
    ZoneKey prepared_;
    std::uint16_t marker_{};
};

[[nodiscard]] entt::entity add_actor(Zone& zone, std::uint64_t uid,
                                     LocalXY position = {1, 2}) {
    const auto entity = zone.reg.create();
    zone.reg.emplace<StableId>(entity, uid);
    zone.reg.emplace<MovementPoints>(entity, 7, 13);
    zone.reg.emplace<LocalPosition>(entity, position);
    return entity;
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

TEST(CrossZoneMigration, CopiesComponentsInvalidatesOldHandleAndSynchronizesUidIndex) {
    PreparedLocalStore store{kDestination, 2};
    auto source = local_zone(kSource, 1);
    const auto old_handle = add_actor(*source, 77);
    ZoneManager manager{store};
    static_cast<void>(manager.adopt(std::move(source)));
    CrossZoneRuntime runtime{manager};

    ASSERT_TRUE(runtime.migrate_entity(kSource, old_handle, kDestination, {60, 61}));
    EXPECT_FALSE(runtime.resolve({kSource, 77}).has_value());
    const auto moved = runtime.resolve({kDestination, 77});
    ASSERT_TRUE(moved.has_value());

    ASSERT_TRUE(manager.with(*manager.get(kSource), [&](const Zone& loaded) {
        EXPECT_FALSE(loaded.reg.valid(old_handle));
        EXPECT_FALSE(loaded.uid_index.contains(77));
    }));
    ASSERT_TRUE(manager.with(*manager.get(kDestination), [&](const Zone& loaded) {
        EXPECT_EQ(loaded.uid_index.at(77), *moved);
        EXPECT_EQ(loaded.reg.get<const StableId>(*moved).uid, 77U);
        EXPECT_EQ(loaded.reg.get<const MovementPoints>(*moved), (MovementPoints{7, 13}));
        EXPECT_EQ(loaded.reg.get<const LocalPosition>(*moved).tile, (LocalXY{60, 61}));
        EXPECT_EQ(loaded.touch_count, 1U);
    }));
}

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

[[nodiscard]] std::array<std::uint64_t, 2> deterministic_migration_hashes() {
    InMemoryZoneStore store{test_ruleset()};
    auto source = local_zone(kSource);
    const auto actor = add_actor(*source, 1234, {1, 1});
    ZoneManager manager{store};
    static_cast<void>(manager.adopt(std::move(source)));
    static_cast<void>(manager.adopt(local_zone(kDestination)));
    CrossZoneRuntime runtime{manager};
    if (!runtime.migrate_entity(kSource, actor, kDestination, {63, 62})) {
        return {};
    }
    const auto moved = runtime.resolve({kDestination, 1234});
    if (!moved.has_value() ||
        !runtime.migrate_entity(kDestination, *moved, kSource, {2, 3})) {
        return {};
    }
    std::array<std::uint64_t, 2> result{};
    static_cast<void>(manager.with(*manager.get(kSource), [&](const Zone& loaded) {
        result[0] = aetheria::serialize::normalized_state_hash(loaded, test_ruleset());
    }));
    static_cast<void>(manager.with(*manager.get(kDestination), [&](const Zone& loaded) {
        result[1] = aetheria::serialize::normalized_state_hash(loaded, test_ruleset());
    }));
    return result;
}

TEST(CrossZoneMigration, SameOperationSequenceHasSameNormalizedHashes) {
    const auto first = deterministic_migration_hashes();
    const auto second = deterministic_migration_hashes();
    EXPECT_EQ(first, second);
    EXPECT_NE(first[0], 0U);
    EXPECT_NE(first[1], 0U);
    std::cout << "cross_zone_normalized_hashes=" << first[0] << ',' << first[1] << '\n';
}

}  // namespace

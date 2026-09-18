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
    [[nodiscard]] std::vector<ZoneKey> stored_keys() const override {
        return {prepared_};
    }
    [[nodiscard]] const std::optional<aetheria::zone::SaveManifest>&
    manifest() const noexcept override {
        return manifest_;
    }
    void write_manifest(const aetheria::zone::SaveManifest& manifest) override {
        manifest_ = manifest;
    }

private:
    ZoneKey prepared_;
    std::uint16_t marker_{};
    std::optional<aetheria::zone::SaveManifest> manifest_;
};

[[nodiscard]] std::array<std::uint64_t, 2> deterministic_migration_hashes() {
    const auto parent = open_site_layer();
    InMemoryZoneStore store{test_ruleset()};
    auto source = local_zone(kSource);
    const auto actor = add_actor(*source, 1234, {1, 1});
    auto destination = local_zone(kDestination);
    static_cast<void>(add_persistent_object(*destination, 4321));
    store.save(*destination);
    ZoneManager manager{store, local_materializer(parent)};
    static_cast<void>(manager.adopt(std::move(source)));
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

TEST(CrossZoneMigration, CopiesComponentsInvalidatesOldHandleAndSynchronizesUidIndex) {
    PreparedLocalStore store{kDestination, 2};
    auto source = local_zone(kSource, 1);
    const auto old_handle = add_actor(*source, 77);
    ZoneManager manager{store, [](ZoneKey, std::unique_ptr<Zone> loaded) {
                            return loaded;
                        }};
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

TEST(CrossZoneMigration, DiskOnlyDestinationIsRematerializedBeforeMove) {
    const auto parent = open_site_layer();
    const auto feature = *test_ruleset().find_feature("feature.none");
    auto destination = aetheria::local::materialize_local_zone(
        kSite, parent, {12, 12}, aetheria::tests::kLocalSiteSeed, feature,
        test_ruleset());
    static_cast<void>(add_persistent_object(destination, 7001));
    InMemoryZoneStore store{test_ruleset()};
    store.save(destination);
    auto source = local_zone(kSource, 1);
    const auto actor = add_actor(*source, 7002);
    ZoneManager manager{store, local_materializer(parent)};
    static_cast<void>(manager.adopt(std::move(source)));
    CrossZoneRuntime runtime{manager};

    ASSERT_TRUE(runtime.migrate_entity(kSource, actor, kDestination, {60, 61}));
    const auto moved = runtime.resolve({kDestination, 7002});
    ASSERT_TRUE(moved.has_value());
    std::size_t tile_count{};
    ASSERT_TRUE(manager.with(*manager.get(kDestination), [&](const Zone& loaded) {
        tile_count = std::get<aetheria::zone::LocalPayload>(loaded.payload)
                         .layers.at(0)
                         .ground.size();
        const auto persistent = runtime.resolve({kDestination, 7001});
        ASSERT_TRUE(persistent.has_value());
        EXPECT_EQ(loaded.reg.get<const StableId>(*persistent).uid, 7001U);
    }));
    EXPECT_EQ(tile_count, aetheria::local::kLocalTileCount);
    std::cout << "cross_zone_disk_only migrated=1 destination_tiles=" << tile_count
              << " persistent_objects=1 moved_uid=7002\n";
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

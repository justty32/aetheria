#include "tests/sim/world_hash_test_support.h"

#include "core/zone/zone_manager.h"

#include <cstdint>
#include <iostream>
#include <string>

#include <gtest/gtest.h>

namespace {

using aetheria::sim::world_state_hash;
using aetheria::tests::create_world_hash_save;
using aetheria::tests::kWorldHashRegionKeys;
using aetheria::tests::saved_zone_bytes_evidence;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::world::MovementPoints;
using aetheria::world::StableId;
using aetheria::zone::FileZoneStore;
using aetheria::zone::LodLevel;
using aetheria::zone::ZoneManager;

TEST(WorldStateHash, DifferentConstructionHistoriesMatchWhileSavedBytesDiffer) {
    TemporaryDirectory forward_directory;
    TemporaryDirectory reverse_directory;
    create_world_hash_save(forward_directory.path(), false);
    create_world_hash_save(reverse_directory.path(), true);

    FileZoneStore forward_store{forward_directory.path(), test_ruleset()};
    ZoneManager manager{forward_store};
    for (const auto key : kWorldHashRegionKeys) {
        EXPECT_FALSE(manager.get(key).has_value());
    }

    const auto forward = world_state_hash(forward_directory.path(), test_ruleset());
    const auto reverse = world_state_hash(reverse_directory.path(), test_ruleset());
    const auto [forward_bytes_hash, forward_bytes] =
        saved_zone_bytes_evidence(forward_directory.path());
    const auto [reverse_bytes_hash, reverse_bytes] =
        saved_zone_bytes_evidence(reverse_directory.path());

    std::cout << "history_forward world_hash=" << forward.hash
              << " bytes_hash=" << forward_bytes_hash << " bytes=" << forward_bytes << '\n'
              << "history_reverse world_hash=" << reverse.hash
              << " bytes_hash=" << reverse_bytes_hash << " bytes=" << reverse_bytes << '\n';
    EXPECT_EQ(forward.zone_count, 4U);
    EXPECT_EQ(reverse.zone_count, 4U);
    EXPECT_EQ(forward.hash, reverse.hash);
    EXPECT_NE(forward_bytes_hash, reverse_bytes_hash);
}

TEST(WorldStateHash, ComponentChangesHashButRuntimeStateDoesNot) {
    TemporaryDirectory directory;
    create_world_hash_save(directory.path());
    const auto original = world_state_hash(directory.path(), test_ruleset());
    FileZoneStore store{directory.path(), test_ruleset()};
    const auto key = kWorldHashRegionKeys.front();
    const auto before_runtime_bytes = aetheria::tests::read_binary(store.path_for(key));

    auto runtime_only = store.load(key);
    ASSERT_NE(runtime_only, nullptr);
    runtime_only->lod = LodLevel::Absent;
    runtime_only->pinned = true;
    auto& surface = std::get<aetheria::zone::RegionPayload>(runtime_only->payload).layers.at(0);
    surface.site.at(0).lod = LodLevel::Full;
    store.save(*runtime_only);
    const auto runtime_hash = world_state_hash(directory.path(), test_ruleset());
    EXPECT_EQ(aetheria::tests::read_binary(store.path_for(key)), before_runtime_bytes);

    auto component_changed = store.load(key);
    ASSERT_NE(component_changed, nullptr);
    const auto units = component_changed->reg.view<const StableId, MovementPoints>();
    bool changed{};
    for (const auto entity : units) {
        if (units.get<const StableId>(entity).uid == 10U) {
            ++units.get<MovementPoints>(entity).current;
            changed = true;
        }
    }
    ASSERT_TRUE(changed);
    store.save(*component_changed);
    const auto component_hash = world_state_hash(directory.path(), test_ruleset());

    std::cout << "negative_control original_hash=" << original.hash
              << " runtime_hash=" << runtime_hash.hash << " component_hash=" << component_hash.hash
              << '\n';
    EXPECT_EQ(original.hash, runtime_hash.hash);
    EXPECT_NE(original.hash, component_hash.hash);
}

}  // namespace

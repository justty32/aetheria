#include "core/serialize/all_components.h"
#include "core/serialize/normalized_state_hash.h"
#include "core/serialize/registry_codec.h"
#include "core/serialize/zone_codec.h"
#include "core/serialize/zone_codec_detail.h"
#include "core/serialize/zone_diplomacy_codec.h"
#include "core/zone/file_zone_store.h"
#include "sim/world_hash.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/zone/zone_test_support.h"

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::Ruleset;
using aetheria::serialize::AllComponents;
using aetheria::serialize::decode_zone;
using aetheria::serialize::normalized_state_hash;
using aetheria::serialize::RegistryOutputArchive;
using aetheria::serialize::save_registry_snapshot;
using aetheria::site::BuildingState;
using aetheria::site::BuildingType;
using aetheria::site::PersistentBuilding;
using aetheria::site::PersistentDungeon;
using aetheria::site::PersistentNamedNpc;
using aetheria::site::SiteOrderState;
using aetheria::tests::kSite;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::zone::FileZoneStore;
using aetheria::zone::SaveManifest;
using aetheria::zone::Zone;

[[nodiscard]] std::unique_ptr<Zone> observation_site() {
    auto site = std::make_unique<Zone>(kSite);
    auto& persistent = std::get<aetheria::zone::SitePayload>(site->payload).layers.persistent;
    persistent.buildings.push_back(
        PersistentBuilding{{7, 9}, BuildingType::SettlementHall, BuildingState::Idle, 321});
    persistent.order = SiteOrderState{45, 25, 40, 10};
    persistent.place_name_key = "place.test_harbor";
    persistent.named_npcs.push_back(
        PersistentNamedNpc{701, "person.test_scout", "place.test_harbor", true, false});
    persistent.dungeons.push_back(PersistentDungeon{801, "place.test_ruin", false, 3});
    return site;
}

void save_world_root(FileZoneStore& store) {
    store.save(Zone{aetheria::zone::kRootZone});
}

TEST(SiteObservationPersistence, EveryObservationFieldChangesWorldHashIndependently) {
    TemporaryDirectory directory;
    const auto raws_hash = aetheria::tests::prepare_test_save_raws(directory.path());
    FileZoneStore store{directory.path(), test_ruleset()};
    save_world_root(store);
    auto site = observation_site();
    store.save(*site);
    store.write_manifest(SaveManifest{.raws_hash = raws_hash});

    const auto baseline = aetheria::sim::world_state_hash(directory.path()).hash;
    auto& persistent = std::get<aetheria::zone::SitePayload>(site->payload).layers.persistent;
    const auto changed_hash = [&](std::string_view name, auto&& change, auto&& restore) {
        change();
        store.save(*site);
        const auto changed =
            aetheria::sim::world_state_hash(directory.path()).hash;
        EXPECT_NE(baseline, changed) << name;
        std::cout << "observation_hash field=" << name << " before=" << baseline
                  << " after=" << changed << '\n';
        restore();
        return changed;
    };

    static_cast<void>(changed_hash(
        "garrison_coverage", [&] { persistent.order->garrison_coverage = 46; },
        [&] { persistent.order->garrison_coverage = 45; }));
    static_cast<void>(changed_hash(
        "patrol_coverage", [&] { persistent.order->patrol_coverage = 26; },
        [&] { persistent.order->patrol_coverage = 25; }));
    static_cast<void>(changed_hash(
        "bandit_pressure", [&] { persistent.order->bandit_pressure = 41; },
        [&] { persistent.order->bandit_pressure = 40; }));
    static_cast<void>(changed_hash(
        "refugee_pressure", [&] { persistent.order->refugee_pressure = 11; },
        [&] { persistent.order->refugee_pressure = 10; }));
    static_cast<void>(changed_hash(
        "missing", [&] { persistent.named_npcs.front().missing = true; },
        [&] { persistent.named_npcs.front().missing = false; }));
    static_cast<void>(changed_hash(
        "cleared", [&] { persistent.dungeons.front().cleared = true; },
        [&] { persistent.dungeons.front().cleared = false; }));
    static_cast<void>(changed_hash(
        "depth", [&] { persistent.dungeons.front().depth = 4; },
        [&] { persistent.dungeons.front().depth = 3; }));

    store.save(*site);
    const auto replay = aetheria::sim::world_state_hash(directory.path()).hash;
    EXPECT_EQ(replay, baseline);
    EXPECT_EQ(aetheria::sim::world_state_hash(directory.path()).hash, replay);
    std::cout << "observation_hash deterministic=" << replay << " repeated=1\n";
}

TEST(SiteObservationPersistence, ColdFileRoundTripPreservesNormalizedHash) {
    TemporaryDirectory directory;
    FileZoneStore store{directory.path(), test_ruleset()};
    save_world_root(store);
    auto source = observation_site();
    auto& source_persistent =
        std::get<aetheria::zone::SitePayload>(source->payload).layers.persistent;
    source_persistent.named_npcs.front().missing = true;
    source_persistent.dungeons.front().cleared = true;
    source_persistent.dungeons.front().depth = 4;
    const auto before = normalized_state_hash(*source, test_ruleset());
    store.save(*source);
    store.write_manifest(SaveManifest{});
    source.reset();
    ASSERT_EQ(source, nullptr);

    const auto loaded = store.load(kSite);
    ASSERT_NE(loaded, nullptr);
    const auto after = normalized_state_hash(*loaded, test_ruleset());
    EXPECT_EQ(after, before);
    const auto& persistent =
        std::get<aetheria::zone::SitePayload>(loaded->payload).layers.persistent;
    ASSERT_TRUE(persistent.order.has_value());
    ASSERT_EQ(persistent.named_npcs.size(), 1U);
    ASSERT_EQ(persistent.dungeons.size(), 1U);
    EXPECT_EQ(*persistent.order, (SiteOrderState{45, 25, 40, 10}));
    EXPECT_TRUE(persistent.named_npcs.front().missing);
    EXPECT_TRUE(persistent.dungeons.front().cleared);
    EXPECT_EQ(persistent.dungeons.front().depth, 4U);
    std::cout << "observation_cold_roundtrip before=" << before << " after=" << after
              << " source_destroyed=1 path=file_decode bytes_equal_not_used=1\n";
}

}  // namespace

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
using aetheria::serialize::RegistryOutputArchive;
using aetheria::serialize::decode_zone;
using aetheria::serialize::normalized_state_hash;
using aetheria::serialize::save_registry_snapshot;
using aetheria::site::BuildingState;
using aetheria::site::BuildingType;
using aetheria::site::PersistentBuilding;
using aetheria::site::PersistentDungeon;
using aetheria::site::PersistentNamedNpc;
using aetheria::site::SiteOrderState;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::kSite;
using aetheria::tests::test_ruleset;
using aetheria::zone::FileZoneStore;
using aetheria::zone::SaveManifest;
using aetheria::zone::Zone;

template <typename Def>
[[nodiscard]] std::vector<std::string> definition_ids(std::span<const Def> definitions) {
    std::vector<std::string> result;
    result.reserve(definitions.size());
    for (const auto& definition : definitions) {
        result.push_back(definition.id);
    }
    return result;
}

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

[[nodiscard]] std::string encode_v16_site_zone(const Ruleset& ruleset) {
    const Zone site{kSite};
    std::ostringstream stream{std::ios::binary};
    {
        cereal::PortableBinaryOutputArchive archive{
            stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
        constexpr std::uint32_t version = 16;
        const auto key = aetheria::zone::value_of(site.key);
        const auto tick = static_cast<std::int64_t>(site.last_saved_tick);
        auto terrains = definition_ids(ruleset.terrains());
        auto reliefs = definition_ids(ruleset.reliefs());
        auto features = definition_ids(ruleset.features());
        auto edges = definition_ids(ruleset.edges());
        constexpr std::uint8_t payload_index = 2;
        const std::vector<PersistentBuilding> legacy_buildings{
            PersistentBuilding{{7, 9}, BuildingType::SettlementHall, BuildingState::Idle, 321}};
        archive(aetheria::serialize::detail::kZoneMagic, version, key, tick,
                aetheria::serialize::detail::kReservedPersistenceFlags, terrains, reliefs, features,
                edges, payload_index, legacy_buildings);
    }
    {
        cereal::PortableBinaryOutputArchive archive{
            stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
        RegistryOutputArchive registry_archive{archive};
        save_registry_snapshot(site.reg, registry_archive, AllComponents{});
    }
    {
        cereal::PortableBinaryOutputArchive archive{
            stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
        aetheria::serialize::detail::save_diplomacy(archive, site.diplomacy, ruleset);
    }
    return std::move(stream).str();
}

void save_world_root(FileZoneStore& store) {
    store.save(Zone{aetheria::zone::kRootZone});
}

TEST(SiteObservationPersistence, V16SiteIsRejectedInsteadOfLoadingNewFieldsAsDefaults) {
    static_assert(aetheria::serialize::kSaveFormatVersion == 18);
    const auto legacy = encode_v16_site_zone(test_ruleset());
    try {
        static_cast<void>(decode_zone(legacy, test_ruleset()));
        FAIL() << "v16 Site payload should be rejected before its legacy fields are decoded";
    } catch (const std::runtime_error& error) {
        const std::string message{error.what()};
        EXPECT_NE(message.find("zone format_version"), std::string::npos);
        EXPECT_NE(message.find("檔內=16"), std::string::npos);
        EXPECT_NE(message.find("預期=18"), std::string::npos);
        std::cout << "site_v16_rejected error=\"" << message << "\"\n";
    }

    const Zone defaults{kSite};
    const auto loaded_defaults = decode_zone(
        aetheria::serialize::encode_zone(defaults, test_ruleset()), test_ruleset());
    const auto& persistent =
        std::get<aetheria::zone::SitePayload>(loaded_defaults->payload).layers.persistent;
    EXPECT_FALSE(persistent.order.has_value());
    EXPECT_TRUE(persistent.named_npcs.empty());
    EXPECT_TRUE(persistent.dungeons.empty());
    std::cout << "site_v18_defaults accepted=1 order_present=0 named_npcs=0 dungeons=0\n";
}

TEST(SiteObservationPersistence, EveryObservationFieldChangesWorldHashIndependently) {
    TemporaryDirectory directory;
    FileZoneStore store{directory.path(), test_ruleset()};
    save_world_root(store);
    auto site = observation_site();
    store.save(*site);
    store.write_manifest(SaveManifest{});

    const auto baseline = aetheria::sim::world_state_hash(directory.path(), test_ruleset()).hash;
    auto& persistent = std::get<aetheria::zone::SitePayload>(site->payload).layers.persistent;
    const auto changed_hash = [&](std::string_view name, auto&& change, auto&& restore) {
        change();
        store.save(*site);
        const auto changed =
            aetheria::sim::world_state_hash(directory.path(), test_ruleset()).hash;
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
    const auto replay = aetheria::sim::world_state_hash(directory.path(), test_ruleset()).hash;
    EXPECT_EQ(replay, baseline);
    EXPECT_EQ(aetheria::sim::world_state_hash(directory.path(), test_ruleset()).hash, replay);
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

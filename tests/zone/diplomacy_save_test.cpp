#include "core/serialize/all_components.h"
#include "core/serialize/normalized_state_hash.h"
#include "core/serialize/registry_codec.h"
#include "core/serialize/zone_codec_detail.h"
#include "core/world/diplomacy.h"
#include "core/zone/file_zone_store.h"
#include "core/zone/save_manifest_io.h"
#include "sim/world_hash.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/zone/zone_test_support.h"

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::Ruleset;
using aetheria::serialize::AllComponents;
using aetheria::serialize::normalized_state_hash;
using aetheria::serialize::RegistryOutputArchive;
using aetheria::serialize::save_registry_snapshot;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::time::kXun;
using aetheria::time::Tick;
using aetheria::world::DiplomaticRelation;
using aetheria::world::FactionId;
using aetheria::world::WorldDiplomacyState;
using aetheria::zone::FileZoneStore;
using aetheria::zone::kRootZone;
using aetheria::zone::SaveManifest;
using aetheria::zone::Zone;

[[nodiscard]] std::vector<std::string> terrain_ids(const Ruleset& ruleset) {
    std::vector<std::string> result;
    result.reserve(ruleset.terrains().size());
    for (const auto& definition : ruleset.terrains()) {
        result.push_back(definition.id);
    }
    return result;
}

template <typename Def>
[[nodiscard]] std::vector<std::string> definition_ids(std::span<const Def> definitions) {
    std::vector<std::string> result;
    result.reserve(definitions.size());
    for (const auto& definition : definitions) {
        result.push_back(definition.id);
    }
    return result;
}

[[nodiscard]] std::string encode_v14_root_zone(const Ruleset& ruleset) {
    const Zone root{kRootZone};
    std::ostringstream stream{std::ios::binary};
    {
        cereal::PortableBinaryOutputArchive archive{
            stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
        constexpr std::uint32_t version = 14;
        const auto key = aetheria::zone::value_of(root.key);
        const auto tick = static_cast<std::int64_t>(root.last_saved_tick);
        auto terrains = terrain_ids(ruleset);
        auto reliefs = definition_ids(ruleset.reliefs());
        auto features = definition_ids(ruleset.features());
        auto edges = definition_ids(ruleset.edges());
        constexpr std::uint8_t payload_index = 0;
        archive(aetheria::serialize::detail::kZoneMagic, version, key, tick,
                aetheria::serialize::detail::kReservedPersistenceFlags, terrains, reliefs, features,
                edges, payload_index);
    }
    {
        cereal::PortableBinaryOutputArchive archive{
            stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
        RegistryOutputArchive registry_archive{archive};
        save_registry_snapshot(root.reg, registry_archive, AllComponents{});
    }
    return std::move(stream).str();
}

void populate_non_default_diplomacy(WorldDiplomacyState& state) {
    state.set_relation(FactionId{1}, FactionId{2},
                       {.favor = 1234, .trust = -2345, .fear = 3456, .grievance = 4567});
    state.set_relation(FactionId{2}, FactionId{1},
                       {.favor = -765, .trust = 876, .fear = -987, .grievance = 1098});
    state.set_relation(FactionId{1}, FactionId{3},
                       {.favor = 111, .trust = -222, .fear = 333, .grievance = -444});

    const auto truce = *test_ruleset().find_treaty("treaty.truce");
    const auto alliance = *test_ruleset().find_treaty("treaty.defensive_alliance");
    static_cast<void>(state.start_treaty(truce, FactionId{1}, FactionId{2},
                                         Tick{2 * static_cast<std::int64_t>(kXun)}));
    static_cast<void>(state.start_treaty(alliance, FactionId{2}, FactionId{3},
                                         Tick{5 * static_cast<std::int64_t>(kXun)}));

    const auto revenge = *test_ruleset().find_casus_belli("casus_belli.revenge");
    static_cast<void>(state.grant_casus_belli(FactionId{1}, FactionId{2}, revenge,
                                              Tick{7 * static_cast<std::int64_t>(kXun)}));
    auto& war = state.declare_war(FactionId{1}, FactionId{2}, revenge,
                                  Tick{8 * static_cast<std::int64_t>(kXun)});
    state.advance_war_xun(war, {2000, 1000});
    state.add_war_score(war, 375);
}

TEST(DiplomacySave, ColdDiskRoundTripPreservesNonDefaultDirectedStateAndHash) {
    TemporaryDirectory directory;
    std::uint64_t before_hash{};
    {
        FileZoneStore store{directory.path(), test_ruleset()};
        auto source = std::make_unique<Zone>(kRootZone);
        source->diplomacy.emplace(3, UINT64_C(0x123456789ABCDEF0), test_ruleset());
        populate_non_default_diplomacy(*source->diplomacy);
        source->last_saved_tick = Tick{12 * static_cast<std::int64_t>(kXun)};
        before_hash = normalized_state_hash(*source, test_ruleset());
        store.save(*source);
        SaveManifest manifest;
        manifest.now = source->last_saved_tick;
        store.write_manifest(manifest);
        source.reset();
        EXPECT_EQ(source, nullptr);
    }

    FileZoneStore cold_store{directory.path(), test_ruleset()};
    auto loaded = cold_store.load(kRootZone);
    ASSERT_NE(loaded, nullptr);
    ASSERT_TRUE(loaded->diplomacy.has_value());
    EXPECT_EQ(loaded->last_saved_tick, Tick{12 * static_cast<std::int64_t>(kXun)});
    const auto after_hash = normalized_state_hash(*loaded, test_ruleset());
    EXPECT_EQ(after_hash, before_hash);

    const auto forward = loaded->diplomacy->relation(FactionId{1}, FactionId{2});
    const auto reverse = loaded->diplomacy->relation(FactionId{2}, FactionId{1});
    EXPECT_EQ(forward, (DiplomaticRelation{1234, -2345, 3456, 4567}));
    EXPECT_EQ(reverse, (DiplomaticRelation{-765, 876, -987, 1098}));
    EXPECT_NE(forward, reverse);

    const auto persisted = loaded->diplomacy->persistent_state();
    ASSERT_EQ(persisted.treaties.size(), 2U);
    EXPECT_EQ(persisted.treaties[0].expires_at, Tick{20 * static_cast<std::int64_t>(kXun)});
    EXPECT_EQ(persisted.treaties[1].expires_at, Tick{113 * static_cast<std::int64_t>(kXun)});
    ASSERT_EQ(persisted.casus_belli.size(), 1U);
    EXPECT_EQ(persisted.casus_belli[0].granted_at, Tick{7 * static_cast<std::int64_t>(kXun)});
    EXPECT_EQ(persisted.casus_belli[0].expires_at, Tick{43 * static_cast<std::int64_t>(kXun)});
    EXPECT_TRUE(loaded->diplomacy->has_casus_belli(FactionId{1}, FactionId{2},
                                                   persisted.casus_belli[0].def,
                                                   Tick{12 * static_cast<std::int64_t>(kXun)}));
    ASSERT_EQ(persisted.wars.size(), 1U);
    EXPECT_TRUE(persisted.wars[0].active);
    EXPECT_EQ(persisted.wars[0].war_score, 375);
    EXPECT_EQ(persisted.wars[0].weariness, (std::array<std::int32_t, 2>{80, 60}));

    Zone replay{kRootZone};
    replay.diplomacy.emplace(3, UINT64_C(0x123456789ABCDEF0), test_ruleset());
    populate_non_default_diplomacy(*replay.diplomacy);
    EXPECT_EQ(normalized_state_hash(replay, test_ruleset()), before_hash);
    const auto first_world_hash =
        aetheria::sim::world_state_hash(directory.path(), test_ruleset()).hash;
    const auto second_world_hash =
        aetheria::sim::world_state_hash(directory.path(), test_ruleset()).hash;
    EXPECT_EQ(first_world_hash, second_world_hash);
    std::cout << "diplomacy_roundtrip before_hash=" << before_hash << " after_hash=" << after_hash
              << " world_hash=" << first_world_hash << " forward=" << forward.favor << ','
              << forward.trust << ',' << forward.fear << ',' << forward.grievance
              << " reverse=" << reverse.favor << ',' << reverse.trust << ',' << reverse.fear << ','
              << reverse.grievance << " treaty_expiry_xun=20,113"
              << " casus_granted_xun=7 remaining_at_xun12=31"
              << " war_score=375 weariness=80,60"
              << " cold_source_destroyed=1 path=load rematerialize=0\n";
}

TEST(DiplomacySave, V14RootLoadsWithDiplomacyAbsentRatherThanNeutral) {
    TemporaryDirectory directory;
    SaveManifest old_manifest;
    old_manifest.format_version = 14;
    aetheria::zone::detail::atomic_replace(
        directory.path() / "root.bin",
        aetheria::zone::detail::compress(encode_v14_root_zone(test_ruleset())));
    aetheria::zone::detail::atomic_replace(directory.path() / "manifest.bin",
                                           aetheria::zone::detail::encode_manifest(old_manifest));

    FileZoneStore store{directory.path(), test_ruleset()};
    auto loaded = store.load(kRootZone);
    ASSERT_NE(loaded, nullptr);
    EXPECT_FALSE(loaded->diplomacy.has_value());

    Zone neutral{kRootZone};
    neutral.diplomacy.emplace(3, 0, test_ruleset());
    const auto absent_hash = normalized_state_hash(*loaded, test_ruleset());
    const auto neutral_hash = normalized_state_hash(neutral, test_ruleset());
    EXPECT_NE(absent_hash, neutral_hash);
    std::cout << "diplomacy_v14 presence=0 absent_hash=" << absent_hash
              << " neutral_presence=1 neutral_hash=" << neutral_hash << '\n';
}

} // namespace

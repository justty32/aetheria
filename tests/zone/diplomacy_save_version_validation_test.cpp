#include "core/serialize/all_components.h"
#include "core/serialize/normalized_state_hash.h"
#include "core/serialize/registry_codec.h"
#include "core/serialize/zone_codec_detail.h"
#include "core/world/diplomacy.h"
#include "core/world/faction_ai.h"
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

#include "tests/zone/diplomacy_save_test_support.h"

namespace {

using aetheria::rules::Ruleset;
using aetheria::serialize::AllComponentsV15;
using aetheria::serialize::normalized_state_hash;
using aetheria::serialize::RegistryOutputArchive;
using aetheria::serialize::save_registry_snapshot;
using aetheria::serialize::ZoneDecodeMode;
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

using namespace aetheria::tests::diplomacy_save;

[[nodiscard]] std::string encode_v15_root_zone(const Ruleset& ruleset) {
    Zone root{kRootZone};
    root.diplomacy.emplace(3, UINT64_C(0x123456789ABCDEF0), ruleset);
    populate_non_default_diplomacy(*root.diplomacy);
    std::ostringstream stream{std::ios::binary};
    {
        cereal::PortableBinaryOutputArchive archive{
            stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
        constexpr std::uint32_t version = 15;
        const auto key = aetheria::zone::value_of(root.key);
        const auto tick = static_cast<std::int64_t>(root.last_saved_tick);
        auto terrains = terrain_ids(ruleset);
        auto reliefs = definition_ids(ruleset.reliefs());
        auto features = definition_ids(ruleset.features());
        auto edges = definition_ids(ruleset.edges());
        constexpr std::uint8_t payload_index = 0;
        archive(aetheria::serialize::detail::kZoneMagic, version, key, tick,
                aetheria::serialize::detail::kReservedPersistenceFlags, terrains,
                reliefs, features, edges, payload_index);
    }
    {
        cereal::PortableBinaryOutputArchive archive{
            stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
        RegistryOutputArchive registry_archive{archive};
        save_registry_snapshot(root.reg, registry_archive, AllComponentsV15{});
    }
    {
        cereal::PortableBinaryOutputArchive archive{
            stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
        const bool present = true;
        const auto state = root.diplomacy->persistent_state();
        archive(present, state.faction_count, state.world_seed);
        archive(static_cast<std::uint64_t>(state.relations.size()));
        for (const auto& relation : state.relations) {
            archive(relation.favor, relation.trust, relation.fear, relation.grievance);
        }
        archive(static_cast<std::uint64_t>(state.treaties.size()));
        for (const auto& treaty : state.treaties) {
            const auto id = ruleset.treaty(treaty.def)->id;
            const auto first = static_cast<std::uint16_t>(treaty.parties[0]);
            const auto second = static_cast<std::uint16_t>(treaty.parties[1]);
            const auto started = static_cast<std::int64_t>(treaty.started);
            const bool has_expiry = treaty.expires_at.has_value();
            const auto expiry = has_expiry ? static_cast<std::int64_t>(*treaty.expires_at) : 0;
            archive(id, first, second, started, has_expiry, expiry);
        }
        archive(static_cast<std::uint64_t>(state.casus_belli.size()));
        for (const auto& claim : state.casus_belli) {
            const auto owner = static_cast<std::uint16_t>(claim.owner);
            const auto target = static_cast<std::uint16_t>(claim.target);
            const auto id = ruleset.casus_belli(claim.def)->id;
            const auto granted = static_cast<std::int64_t>(claim.granted_at);
            const auto expires = static_cast<std::int64_t>(claim.expires_at);
            archive(owner, target, id, granted, expires);
        }
        archive(static_cast<std::uint64_t>(state.wars.size()));
        for (const auto& war : state.wars) {
            const auto first = static_cast<std::uint16_t>(war.participants[0]);
            const auto second = static_cast<std::uint16_t>(war.participants[1]);
            const bool has_cause = war.cause.has_value();
            const auto cause_id = has_cause ? ruleset.casus_belli(*war.cause)->id : std::string{};
            const auto started = static_cast<std::int64_t>(war.started);
            archive(first, second, has_cause, cause_id, started, war.war_score,
                    war.weariness[0], war.weariness[1], war.active);
        }
    }
    return std::move(stream).str();
}

TEST(DiplomacySave, V15FixtureKeepsAllV18FieldsAbsent) {
    const auto loaded = aetheria::serialize::decode_zone(
        encode_v15_root_zone(test_ruleset()), test_ruleset(), ZoneDecodeMode::LegacyFixture);
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->key, kRootZone);
    ASSERT_TRUE(loaded->diplomacy.has_value());
    EXPECT_TRUE(loaded->reg.view<const aetheria::world::NamedFateLedger>().empty());
    const auto state = loaded->diplomacy->persistent_state();
    EXPECT_FALSE(state.faction_truths.has_value());
    EXPECT_FALSE(state.knowledge.has_value());
    EXPECT_FALSE(state.faction_minds.has_value());
    std::cout << "diplomacy_v15 named_fate=0 truth_presence=0 knowledge_presence=0"
                 " mind_presence=0\n";
}

TEST(DiplomacySave, CurrentStoreRejectsV15Manifest) {
    TemporaryDirectory directory;
    SaveManifest old_manifest;
    old_manifest.format_version = 15;
    aetheria::zone::detail::atomic_replace(
        directory.path() / "root.bin",
        aetheria::zone::detail::compress(encode_v15_root_zone(test_ruleset())));
    aetheria::zone::detail::atomic_replace(directory.path() / "manifest.bin",
                                           aetheria::zone::detail::encode_manifest(old_manifest));
    try {
        static_cast<void>(FileZoneStore{directory.path(), test_ruleset()});
        FAIL() << "v15 manifest should be rejected by v23 store";
    } catch (const std::runtime_error& error) {
        std::cout << "diplomacy_v15_reject_error=" << error.what() << '\n';
        EXPECT_NE(std::string{error.what()}.find("檔內=15 預期=23"), std::string::npos);
    }
}

TEST(DiplomacySave, CurrentStoreRejectsV15ZoneAfterOpening) {
    TemporaryDirectory directory;
    FileZoneStore store{directory.path(), test_ruleset()};
    aetheria::zone::detail::atomic_replace(
        store.path_for(kRootZone),
        aetheria::zone::detail::compress(encode_v15_root_zone(test_ruleset())));
    try {
        static_cast<void>(store.load(kRootZone));
        FAIL() << "v15 zone should be rejected by an already-open v23 store";
    } catch (const std::runtime_error& error) {
        std::cout << "diplomacy_v15_zone_reject_error=" << error.what() << '\n';
        EXPECT_NE(std::string{error.what()}.find("檔內=15 預期=23"), std::string::npos);
    }
}

}  // namespace

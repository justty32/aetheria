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
    state.set_faction_truth(FactionId{1}, 1357, 2468);
    state.set_faction_truth(FactionId{2}, 3579, 4680);
    state.set_faction_truth(FactionId{3}, 5791, 6802);
    state.observe_faction(FactionId{1}, FactionId{2}, 3200,
                          Tick{3 * static_cast<std::int64_t>(kXun)}, 11);
    state.observe_faction(FactionId{1}, FactionId{3}, 4700,
                          Tick{4 * static_cast<std::int64_t>(kXun)}, 19);
    auto& mind = state.faction_mind(FactionId{1});
    mind.goal = aetheria::ai::FactionGoal::Conquer;
    mind.goal_score = 777;
    mind.goal_switches = 9;
    mind.initialized = true;
    mind.forced_goal = aetheria::ai::FactionGoal::Conquer;
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
        save_registry_snapshot(root.reg, registry_archive, AllComponents{});
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
    ASSERT_TRUE(persisted.faction_truths.has_value());
    ASSERT_TRUE(persisted.knowledge.has_value());
    ASSERT_TRUE(persisted.faction_minds.has_value());
    EXPECT_EQ(loaded->diplomacy->faction_truth(FactionId{2})->military_power, 3579);
    const auto loaded_view =
        aetheria::world::make_faction_view(*loaded->diplomacy, FactionId{1});
    ASSERT_EQ(loaded_view.estimates().size(), 2U);
    EXPECT_EQ(loaded_view.estimate(2)->route_cost, 11);
    EXPECT_EQ(loaded->diplomacy->faction_mind(FactionId{1}).goal_switches, 9U);
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
    const auto loaded_decision = aetheria::world::plan_faction_ai_xun(
        *loaded->diplomacy, FactionId{1}, {3, 0, true, true, false},
        Tick{12 * static_cast<std::int64_t>(kXun)}, test_ruleset());
    const auto replay_decision = aetheria::world::plan_faction_ai_xun(
        *replay.diplomacy, FactionId{1}, {3, 0, true, true, false},
        Tick{12 * static_cast<std::int64_t>(kXun)}, test_ruleset());
    EXPECT_EQ(loaded_decision.decision.command, replay_decision.decision.command);
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
              << " truth2=3579 knowledge_records=2 mind_switches=9 decision="
              << static_cast<int>(loaded_decision.decision.command.kind)
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

TEST(DiplomacySave, V15KeepsNewKnowledgeAndMindFieldsAbsent) {
    TemporaryDirectory directory;
    SaveManifest old_manifest;
    old_manifest.format_version = 15;
    aetheria::zone::detail::atomic_replace(
        directory.path() / "root.bin",
        aetheria::zone::detail::compress(encode_v15_root_zone(test_ruleset())));
    aetheria::zone::detail::atomic_replace(directory.path() / "manifest.bin",
                                           aetheria::zone::detail::encode_manifest(old_manifest));
    FileZoneStore store{directory.path(), test_ruleset()};
    auto loaded = store.load(kRootZone);
    ASSERT_NE(loaded, nullptr);
    ASSERT_TRUE(loaded->diplomacy.has_value());
    const auto state = loaded->diplomacy->persistent_state();
    EXPECT_FALSE(state.faction_truths.has_value());
    EXPECT_FALSE(state.knowledge.has_value());
    EXPECT_FALSE(state.faction_minds.has_value());
    EXPECT_FALSE(loaded->diplomacy->faction_truth(FactionId{1}).has_value());
    EXPECT_TRUE(aetheria::world::make_faction_view(*loaded->diplomacy, FactionId{1})
                    .estimates()
                    .empty());
    std::cout << "diplomacy_v15 truth_presence=0 knowledge_presence=0 mind_presence=0\n";
}

} // namespace

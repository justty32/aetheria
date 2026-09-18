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



TEST(DiplomacySave, ColdDiskRoundTripPreservesNonDefaultDirectedStateAndHash) {
    TemporaryDirectory directory;
    const auto raws_hash = aetheria::tests::prepare_test_save_raws(directory.path());
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
        manifest.raws_hash = raws_hash;
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
        aetheria::sim::world_state_hash(directory.path()).hash;
    const auto second_world_hash =
        aetheria::sim::world_state_hash(directory.path()).hash;
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

}  // namespace

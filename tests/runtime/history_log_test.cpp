#include "core/history/history_log.h"
#include "core/runtime/playable_session.h"
#include "core/runtime/save_raws.h"
#include "core/runtime/session_persistence.h"
#include "core/zone/file_zone_store.h"
#include "core/zone/save_manifest_io.h"
#include "sim/world_hash.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/zone/zone_test_support.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>

#include "tests/runtime/history_log_test_support.h"

namespace {

using aetheria::runtime::PlayableBattleChoice;
using aetheria::runtime::PlayableSession;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;

using namespace aetheria::tests::history_log;

void play_battle_and_coverage(PlayableSession& session) {
    session.issue_move(session.player_army_id(), session.guided_target());
    EXPECT_FALSE(session.advance_xun().encounter_pending);
    EXPECT_TRUE(session.advance_xun().encounter_pending);
    static_cast<void>(session.resolve_encounter(PlayableBattleChoice::CommandSite));
    session.enter_site();
    session.build_city();
    session.leave_site();
}

[[nodiscard]] std::uint64_t replayed_identity(
    const std::filesystem::path& source_slot,
    const std::filesystem::path& replay_slot) {
    const auto raws_hash = aetheria::runtime::save_raws_hash(source_slot);
    aetheria::zone::FileZoneStore source{source_slot, test_ruleset()};
    const auto metadata = aetheria::runtime::inspect_session_save(source);
    const aetheria::history::HistoryLog source_history{source_slot / "history.log"};
    aetheria::zone::InMemoryZoneStore active{test_ruleset()};
    PlayableSession replay{metadata.world_seed, metadata.region_id,
                           aetheria::runtime::save_raws_directory(source_slot).string(),
                           active};
    replay.replay_history_from(source_slot);
    aetheria::zone::FileZoneStore destination{replay_slot, test_ruleset()};
    replay.save_game(destination, "replay");
    const auto replayed = aetheria::sim::world_state_hash(replay_slot);
    return aetheria::sim::compose_world_identity(
        replayed.zone_hash, raws_hash, source_history.head_hash());
}

TEST(HistoryAcceptance, ActiveColdAndReplayWorldIdentityMatch) {
    TemporaryDirectory workspace;
    const auto slot = workspace.path() / "world";
    aetheria::zone::InMemoryZoneStore active{test_ruleset()};
    PlayableSession session{UINT64_C(515151), 51, AETHERIA_SOURCE_DIR "/data", active};
    play_battle_and_coverage(session);
    aetheria::zone::FileZoneStore destination{slot, test_ruleset()};
    session.save_game(destination, "default");
    const auto active_hash = aetheria::sim::world_state_hash(slot).hash;

    aetheria::zone::FileZoneStore cold_store{slot, test_ruleset()};
    auto cold = PlayableSession::load(slot, cold_store, "default");
    const auto cold_hash = aetheria::sim::world_state_hash(slot).hash;
    const auto replay_hash = replayed_identity(slot, workspace.path() / "replayed");
    EXPECT_EQ(active_hash, cold_hash);
    EXPECT_EQ(cold_hash, replay_hash);
    EXPECT_EQ(aetheria::sim::run_replay(slot), 0);
    std::cout << "acceptance1 active_hash=" << active_hash
              << " cold_hash=" << cold_hash
              << " replay_hash=" << replay_hash << '\n';
}

TEST(HistoryAcceptance, MiddleByteTamperFailsAtExactEntryAndRestorePasses) {
    TemporaryDirectory workspace;
    const auto slot = workspace.path() / "world";
    {
        aetheria::zone::InMemoryZoneStore active{test_ruleset()};
        PlayableSession session{UINT64_C(515151), 51, AETHERIA_SOURCE_DIR "/data", active};
        play_battle_and_coverage(session);
        aetheria::zone::FileZoneStore destination{slot, test_ruleset()};
        session.save_game(destination, "default");
    }
    const auto path = slot / "history.log";
    const auto original = read_bytes(path);
    auto tampered = original;
    const auto position = tampered.find("advance_xun");
    ASSERT_NE(position, std::string::npos);
    tampered[position + 1U] ^= 1;
    write_bytes(path, tampered);

    std::string failure;
    try {
        aetheria::zone::FileZoneStore store{slot, test_ruleset()};
        static_cast<void>(PlayableSession::load(slot, store, "default"));
        FAIL() << "tampered history should fail";
    } catch (const std::runtime_error& error) {
        failure = error.what();
    }
    EXPECT_NE(failure.find("第 2 筆斷裂"), std::string::npos);
    std::cout << "acceptance2 tampered=RED error=" << failure << '\n';

    write_bytes(path, original);
    aetheria::zone::FileZoneStore restored_store{slot, test_ruleset()};
    auto restored = PlayableSession::load(slot, restored_store, "default");
    EXPECT_GT(restored->history_head_seq(), 0U);
    std::cout << "acceptance2 restored=GREEN history_seq="
              << restored->history_head_seq() << '\n';
}

TEST(HistoryAcceptance, EmptyWorldPreservesV23ZoneAndFoldsThreeComponents) {
    constexpr std::uint64_t kV23ZoneHash = UINT64_C(17114528469974780418);
    TemporaryDirectory workspace;
    const auto empty_slot = workspace.path() / "empty";
    {
        aetheria::zone::InMemoryZoneStore active{test_ruleset()};
        PlayableSession session{UINT64_C(999999), 51, AETHERIA_SOURCE_DIR "/data", active};
        aetheria::zone::FileZoneStore destination{empty_slot, test_ruleset()};
        session.save_game(destination, "default");
    }
    const auto report = aetheria::sim::world_state_hash(empty_slot);
    const auto independently_folded = fold_identity_for_test(
        report.zone_hash, report.raws_hash, report.history_head_hash);
    EXPECT_EQ(report.zone_hash, kV23ZoneHash);
    EXPECT_EQ(report.history_head_seq, 0U);
    EXPECT_EQ(report.history_head_hash,
              aetheria::history::kHistoryGenesisHash);
    EXPECT_EQ(report.hash, independently_folded);
    std::cout << "acceptance_hash zone_v23=" << kV23ZoneHash
              << " zone_actual=" << report.zone_hash << '\n'
              << "acceptance_hash empty_head_seq=" << report.history_head_seq
              << " empty_head_hash=" << report.history_head_hash << '\n'
              << "acceptance_hash folded_expected=" << independently_folded
              << " composed_actual=" << report.hash << '\n';
}

}  // namespace

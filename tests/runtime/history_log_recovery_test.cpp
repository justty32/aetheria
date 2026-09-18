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



TEST(HistoryAcceptance, JournalBeforeApplyInterruptionRecoversBitExactly) {
    TemporaryDirectory workspace;
    const auto interrupted_slot = workspace.path() / "interrupted";
    const auto direct_slot = workspace.path() / "direct";
    {
        aetheria::zone::InMemoryZoneStore active{test_ruleset()};
        PlayableSession session{UINT64_C(515151), 51, AETHERIA_SOURCE_DIR "/data", active};
        aetheria::zone::FileZoneStore destination{interrupted_slot, test_ruleset()};
        session.save_game(destination, "default");
    }
    {
        aetheria::zone::FileZoneStore store{interrupted_slot, test_ruleset()};
        auto session = PlayableSession::load(interrupted_slot, store, "default");
        session->set_interrupt_after_journal_for_testing(true);
        try {
            session->issue_move(session->player_army_id(), session->coverage_tile());
            FAIL() << "interrupt hook should throw";
        } catch (const std::runtime_error& error) {
            std::cout << "acceptance3 interrupted=RED error=" << error.what() << '\n';
        }
    }
    {
        aetheria::zone::InMemoryZoneStore active{test_ruleset()};
        PlayableSession session{UINT64_C(515151), 51, AETHERIA_SOURCE_DIR "/data", active};
        session.issue_move(session.player_army_id(), session.coverage_tile());
        aetheria::zone::FileZoneStore destination{direct_slot, test_ruleset()};
        session.save_game(destination, "default");
    }
    aetheria::zone::FileZoneStore recovered_store{interrupted_slot, test_ruleset()};
    auto recovered = PlayableSession::load(interrupted_slot, recovered_store, "default");
    const auto recovered_hash =
        aetheria::sim::world_state_hash(interrupted_slot).hash;
    const auto direct_hash = aetheria::sim::world_state_hash(direct_slot).hash;
    EXPECT_EQ(recovered_hash, direct_hash);
    std::cout << "acceptance3 recovered=GREEN recovered_hash=" << recovered_hash
              << " direct_hash=" << direct_hash << '\n';
}

TEST(HistoryAcceptance, WorldSaveBeforeMarkerInterruptionRebuildsWithoutDuplicateApply) {
    TemporaryDirectory workspace;
    const auto interrupted_slot = workspace.path() / "interrupted-after-save";
    const auto direct_slot = workspace.path() / "direct-after-save";
    {
        aetheria::zone::InMemoryZoneStore active{test_ruleset()};
        PlayableSession session{UINT64_C(515151), 51, AETHERIA_SOURCE_DIR "/data", active};
        aetheria::zone::FileZoneStore destination{interrupted_slot, test_ruleset()};
        session.save_game(destination, "default");
    }
    ASSERT_TRUE(std::filesystem::remove(interrupted_slot / "history.commit"));
    ASSERT_TRUE(std::filesystem::remove(interrupted_slot / "chars" / "default.bin"));
    {
        aetheria::zone::FileZoneStore store{interrupted_slot, test_ruleset()};
        auto session = PlayableSession::load(interrupted_slot, store, "default");
        std::cout << "acceptance_fix2 initial_marker_missing=GREEN history_seq="
                  << session->history_head_seq() << '\n';
        session->enter_site();
        session->build_city();
        session->leave_site();
        session->set_interrupt_after_save_for_testing(true);
        try {
            session->save_game(store, "default");
            FAIL() << "post-save interrupt hook should throw";
        } catch (const std::runtime_error& error) {
            std::cout << "acceptance_fix2 post_save_interrupted=RED error="
                      << error.what() << '\n';
        }
    }
    EXPECT_EQ(read_bytes(interrupted_slot / "history.commit"),
              std::string(sizeof(std::uint64_t), '\0'));

    {
        aetheria::zone::InMemoryZoneStore active{test_ruleset()};
        PlayableSession session{UINT64_C(515151), 51, AETHERIA_SOURCE_DIR "/data", active};
        session.enter_site();
        session.build_city();
        session.leave_site();
        aetheria::zone::FileZoneStore destination{direct_slot, test_ruleset()};
        session.save_game(destination, "default");
    }

    aetheria::zone::FileZoneStore recovered_store{interrupted_slot,
                                                   test_ruleset()};
    auto recovered =
        PlayableSession::load(interrupted_slot, recovered_store, "default");
    const auto recovered_hash =
        aetheria::sim::world_state_hash(interrupted_slot).hash;
    const auto direct_hash = aetheria::sim::world_state_hash(direct_slot).hash;
    EXPECT_EQ(recovered_hash, direct_hash);
    EXPECT_EQ(recovered->history_head_seq(), 3U);
    std::cout << "acceptance_fix2 post_save_recovered=GREEN recovered_hash="
              << recovered_hash << " direct_hash=" << direct_hash
              << " history_seq=" << recovered->history_head_seq() << '\n';
}

}  // namespace

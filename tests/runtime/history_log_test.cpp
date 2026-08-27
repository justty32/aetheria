#include "core/history/history_log.h"
#include "core/runtime/playable_session.h"
#include "core/runtime/save_raws.h"
#include "core/runtime/session_persistence.h"
#include "core/zone/file_zone_store.h"
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

namespace {

using aetheria::runtime::PlayableBattleChoice;
using aetheria::runtime::PlayableSession;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;

[[nodiscard]] std::string read_bytes(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary | std::ios::ate};
    if (!stream) {
        throw std::runtime_error{"測試無法讀檔：" + path.string()};
    }
    const auto end = stream.tellg();
    if (end < 0) {
        throw std::runtime_error{"測試無法取得檔案大小"};
    }
    std::string bytes(static_cast<std::size_t>(end), '\0');
    stream.seekg(0);
    if (!bytes.empty()) {
        stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    if (!stream) {
        throw std::runtime_error{"測試無法完整讀檔"};
    }
    return bytes;
}

void write_bytes(const std::filesystem::path& path, std::string_view bytes) {
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw std::runtime_error{"測試無法寫檔"};
    }
}

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
    const aetheria::history::HistoryLog source_history{
        source_slot / "history.log", raws_hash};
    aetheria::zone::InMemoryZoneStore active{test_ruleset()};
    PlayableSession replay{metadata.world_seed, metadata.region_id,
                           aetheria::runtime::save_raws_directory(source_slot).string(),
                           active};
    replay.replay_history_from(source_slot);
    aetheria::zone::FileZoneStore destination{replay_slot, test_ruleset()};
    replay.save_game(destination, "replay");
    const auto zone_identity = aetheria::sim::world_state_hash(replay_slot).hash;
    return zone_identity ^ raws_hash ^ source_history.head_hash();
}

[[nodiscard]] auto layer_fields(const aetheria::world::LayerCombatResult& value) {
    return std::tuple{
        value.layer, value.resolution_id, value.region_expected_loss_a,
        value.region_expected_loss_b, value.loss_a, value.loss_b,
        value.contribution_b.statistical_loss, value.contribution_b.delta_limit,
        value.contribution_b.requested_deviation,
        value.contribution_b.applied_deviation, value.contribution_b.final_loss,
        value.contribution_b.named_loss, value.contribution_b.unnamed_loss};
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

TEST(HistoryAcceptance, NoCommandWorldMatchesV23AndCommandNegativeDiffers) {
    constexpr std::uint64_t kV23EmptyHash = UINT64_C(17114528469974780418);
    TemporaryDirectory workspace;
    const auto changed_slot = workspace.path() / "changed";
    {
        aetheria::zone::InMemoryZoneStore active{test_ruleset()};
        PlayableSession session{UINT64_C(999999), 51, AETHERIA_SOURCE_DIR "/data", active};
        session.issue_move(session.player_army_id(), session.coverage_tile());
        aetheria::zone::FileZoneStore destination{changed_slot, test_ruleset()};
        session.save_game(destination, "default");
    }
    const auto changed_hash = aetheria::sim::world_state_hash(changed_slot).hash;
    EXPECT_NE(changed_hash, kV23EmptyHash);
    std::cout << "acceptance4 command_injected=RED v23_hash=" << kV23EmptyHash
              << " actual_hash=" << changed_hash << '\n';

    const auto empty_slot = workspace.path() / "empty";
    {
        aetheria::zone::InMemoryZoneStore active{test_ruleset()};
        PlayableSession session{UINT64_C(999999), 51, AETHERIA_SOURCE_DIR "/data", active};
        aetheria::zone::FileZoneStore destination{empty_slot, test_ruleset()};
        session.save_game(destination, "default");
    }
    const auto empty_hash = aetheria::sim::world_state_hash(empty_slot).hash;
    EXPECT_EQ(empty_hash, kV23EmptyHash);
    std::cout << "acceptance4 command_restored=GREEN v23_hash=" << kV23EmptyHash
              << " actual_hash=" << empty_hash << '\n';
}

TEST(HistoryAcceptance, ManifestAllocatorSurvivesColdReadWithoutCollision) {
    TemporaryDirectory workspace;
    const auto slot = workspace.path() / "world";
    std::uint64_t player{};
    std::uint64_t enemy{};
    {
        aetheria::zone::InMemoryZoneStore active{test_ruleset()};
        PlayableSession session{UINT64_C(515151), 51, AETHERIA_SOURCE_DIR "/data", active};
        const auto armies = session.armies();
        ASSERT_EQ(armies.size(), 2U);
        player = session.player_army_id().uid;
        enemy = session.armies().at(0).id.uid == player
                    ? session.armies().at(1).id.uid
                    : session.armies().at(0).id.uid;
        aetheria::zone::FileZoneStore destination{slot, test_ruleset()};
        session.save_game(destination, "default");
    }
    aetheria::zone::FileZoneStore first_cold{slot, test_ruleset()};
    const auto third = first_cold.allocate_entity_uid();
    first_cold.write_manifest(*first_cold.manifest());
    aetheria::zone::FileZoneStore second_cold{slot, test_ruleset()};
    const auto fourth = second_cold.allocate_entity_uid();
    EXPECT_EQ((std::array{player, enemy, third, fourth}),
              (std::array<std::uint64_t, 4>{1001, 2001, 2002, 2003}));
    std::cout << "acceptance5 uid_sequence=" << player << ',' << enemy << ','
              << third << ',' << fourth << '\n';
}

TEST(HistoryAcceptance, CombatAfterColdReadMatchesUninterruptedFieldByField) {
    TemporaryDirectory workspace;
    const auto uninterrupted_slot = workspace.path() / "uninterrupted";
    aetheria::world::LayerCombatResult uninterrupted;
    {
        aetheria::zone::InMemoryZoneStore active{test_ruleset()};
        PlayableSession session{UINT64_C(515151), 51, AETHERIA_SOURCE_DIR "/data", active};
        session.issue_move(session.player_army_id(), session.guided_target());
        ASSERT_FALSE(session.advance_xun().encounter_pending);
        aetheria::zone::FileZoneStore destination{uninterrupted_slot, test_ruleset()};
        session.save_game(destination, "default");
        ASSERT_TRUE(session.advance_xun().encounter_pending);
        uninterrupted = session.resolve_encounter(PlayableBattleChoice::CommandSite).layer_result;
    }

    const auto cold_slot = workspace.path() / "cold";
    {
        aetheria::zone::InMemoryZoneStore active{test_ruleset()};
        PlayableSession session{UINT64_C(515151), 51, AETHERIA_SOURCE_DIR "/data", active};
        session.issue_move(session.player_army_id(), session.guided_target());
        ASSERT_FALSE(session.advance_xun().encounter_pending);
        aetheria::zone::FileZoneStore destination{cold_slot, test_ruleset()};
        session.save_game(destination, "default");
    }
    aetheria::zone::FileZoneStore cold_store{cold_slot, test_ruleset()};
    auto cold = PlayableSession::load(cold_slot, cold_store, "default");
    ASSERT_TRUE(cold->advance_xun().encounter_pending);
    const auto after_cold =
        cold->resolve_encounter(PlayableBattleChoice::CommandSite).layer_result;
    EXPECT_EQ(layer_fields(uninterrupted), layer_fields(after_cold));
    std::cout << "acceptance6 uninterrupted={resolution="
              << uninterrupted.resolution_id << ",expected="
              << uninterrupted.region_expected_loss_a << '/'
              << uninterrupted.region_expected_loss_b << ",loss="
              << uninterrupted.loss_a << '/' << uninterrupted.loss_b
              << ",contribution=" << uninterrupted.contribution_b.statistical_loss
              << '/' << uninterrupted.contribution_b.delta_limit << '/'
              << uninterrupted.contribution_b.requested_deviation << '/'
              << uninterrupted.contribution_b.applied_deviation << '/'
              << uninterrupted.contribution_b.final_loss << '/'
              << uninterrupted.contribution_b.named_loss << '/'
              << uninterrupted.contribution_b.unnamed_loss << "} cold={resolution="
              << after_cold.resolution_id << ",expected="
              << after_cold.region_expected_loss_a << '/'
              << after_cold.region_expected_loss_b << ",loss="
              << after_cold.loss_a << '/' << after_cold.loss_b << "}\n";
}

}  // namespace

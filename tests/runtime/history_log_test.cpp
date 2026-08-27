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

void fold_u64_for_test(std::uint64_t& hash, std::uint64_t value) noexcept {
    constexpr auto kFnvPrime = UINT64_C(1099511628211);
    for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
        hash ^= static_cast<std::uint8_t>(value & UINT8_MAX);
        hash *= kFnvPrime;
        value >>= 8U;
    }
}

[[nodiscard]] std::uint64_t fold_identity_for_test(
    std::uint64_t zone_hash, std::uint64_t raws_hash,
    std::uint64_t head_hash) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    fold_u64_for_test(hash, zone_hash);
    fold_u64_for_test(hash, raws_hash);
    fold_u64_for_test(hash, head_hash);
    return hash;
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

TEST(HistoryAcceptance, EveryIdentityComponentAndFoldOrderChangesComposition) {
    TemporaryDirectory workspace;
    const auto baseline_slot = workspace.path() / "baseline";
    {
        aetheria::zone::InMemoryZoneStore active{test_ruleset()};
        PlayableSession session{UINT64_C(999999), 51, AETHERIA_SOURCE_DIR "/data", active};
        aetheria::zone::FileZoneStore destination{baseline_slot, test_ruleset()};
        session.save_game(destination, "default");
    }
    const auto baseline = aetheria::sim::world_state_hash(baseline_slot);

    const auto zone_slot = workspace.path() / "zone";
    std::filesystem::copy(baseline_slot, zone_slot,
                          std::filesystem::copy_options::recursive);
    std::filesystem::path changed_zone_path;
    std::string original_zone_bytes;
    {
        aetheria::zone::FileZoneStore store{zone_slot, test_ruleset()};
        const auto keys = store.stored_keys();
        const auto region = std::ranges::find_if(
            keys, [](const auto key) { return key != aetheria::zone::kRootZone; });
        ASSERT_NE(region, keys.end());
        auto changed = store.load(*region);
        ASSERT_NE(changed, nullptr);
        changed_zone_path = store.path_for(*region);
        original_zone_bytes = read_bytes(changed_zone_path);
        auto& tiles =
            std::get<aetheria::zone::RegionPayload>(changed->payload).layers.at(0);
        ++tiles.temperature.at(0);
        store.save(*changed);
    }
    const auto zone_changed = aetheria::sim::world_state_hash(zone_slot);
    EXPECT_NE(zone_changed.zone_hash, baseline.zone_hash);
    EXPECT_EQ(zone_changed.raws_hash, baseline.raws_hash);
    EXPECT_EQ(zone_changed.history_head_hash, baseline.history_head_hash);
    EXPECT_NE(zone_changed.hash, baseline.hash);
    write_bytes(changed_zone_path, original_zone_bytes);
    const auto zone_restored = aetheria::sim::world_state_hash(zone_slot);
    EXPECT_EQ(zone_restored.hash, baseline.hash);
    std::cout << "acceptance_components zone_injected=RED before=" << baseline.hash
              << " after=" << zone_changed.hash
              << " zone_restored=GREEN actual=" << zone_restored.hash << '\n';

    const auto raws_slot = workspace.path() / "raws";
    std::filesystem::copy(baseline_slot, raws_slot,
                          std::filesystem::copy_options::recursive);
    const auto manifest_path = raws_slot / "manifest.bin";
    const auto original_manifest_bytes = read_bytes(manifest_path);
    auto manifest = aetheria::zone::detail::decode_manifest(
        original_manifest_bytes);
    const auto terrain_path =
        aetheria::runtime::save_raws_directory(raws_slot) / "terrain.toml";
    const auto original_terrain = read_bytes(terrain_path);
    write_bytes(terrain_path, original_terrain +
                                  "\n# world identity negative control\n");
    manifest.raws_hash = aetheria::runtime::save_raws_hash(raws_slot);
    aetheria::zone::detail::atomic_replace(
        manifest_path, aetheria::zone::detail::encode_manifest(manifest));
    const auto raws_changed = aetheria::sim::world_state_hash(raws_slot);
    EXPECT_EQ(raws_changed.zone_hash, baseline.zone_hash);
    EXPECT_NE(raws_changed.raws_hash, baseline.raws_hash);
    EXPECT_EQ(raws_changed.history_head_hash, baseline.history_head_hash);
    EXPECT_NE(raws_changed.hash, baseline.hash);
    write_bytes(terrain_path, original_terrain);
    aetheria::zone::detail::atomic_replace(manifest_path,
                                            original_manifest_bytes);
    const auto raws_restored = aetheria::sim::world_state_hash(raws_slot);
    EXPECT_EQ(raws_restored.hash, baseline.hash);
    std::cout << "acceptance_components raws_injected=RED before=" << baseline.hash
              << " after=" << raws_changed.hash
              << " raws_restored=GREEN actual=" << raws_restored.hash << '\n';

    const auto history_slot = workspace.path() / "history";
    std::filesystem::copy(baseline_slot, history_slot,
                          std::filesystem::copy_options::recursive);
    const auto history_path = history_slot / "history.log";
    const auto original_history = read_bytes(history_path);
    aetheria::history::HistoryLog history{history_path};
    static_cast<void>(history.append(aetheria::time::Tick{1},
                                     "identity_negative_control", "value = 1\n"));
    const auto history_changed = aetheria::sim::world_state_hash(history_slot);
    EXPECT_EQ(history_changed.zone_hash, baseline.zone_hash);
    EXPECT_EQ(history_changed.raws_hash, baseline.raws_hash);
    EXPECT_NE(history_changed.history_head_hash, baseline.history_head_hash);
    EXPECT_NE(history_changed.hash, baseline.hash);
    write_bytes(history_path, original_history);
    const auto history_restored = aetheria::sim::world_state_hash(history_slot);
    EXPECT_EQ(history_restored.hash, baseline.hash);
    std::cout << "acceptance_components head_injected=RED before=" << baseline.hash
              << " after=" << history_changed.hash
              << " head_restored=GREEN actual=" << history_restored.hash << '\n';

    const auto reordered = fold_identity_for_test(
        baseline.raws_hash, baseline.zone_hash, baseline.history_head_hash);
    const auto canonical = fold_identity_for_test(
        baseline.zone_hash, baseline.raws_hash, baseline.history_head_hash);
    EXPECT_NE(reordered, baseline.hash);
    EXPECT_EQ(canonical, baseline.hash);
    std::cout << "acceptance_components order_swapped=RED canonical=" << baseline.hash
              << " swapped=" << reordered
              << " order_restored=GREEN actual=" << canonical << '\n';
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
              (std::array<std::uint64_t, 4>{1001, 2001, 9002, 9003}));
    std::cout << "acceptance5 uid_sequence=" << player << ',' << enemy << ','
              << third << ',' << fourth << '\n';
}

TEST(HistoryAcceptance, NamedCommanderUidIsReservedBeforeLaterAllocations) {
    aetheria::zone::InMemoryZoneStore active{test_ruleset()};
    PlayableSession session{UINT64_C(515151), 51, AETHERIA_SOURCE_DIR "/data", active};
    const auto later_uid = active.allocate_entity_uid(UINT64_C(9002));

    std::string failure;
    try {
        session.issue_move(session.player_army_id(), session.guided_target());
        EXPECT_FALSE(session.advance_xun().encounter_pending);
        EXPECT_TRUE(session.advance_xun().encounter_pending);
        static_cast<void>(
            session.resolve_encounter(PlayableBattleChoice::CommandSite));
    } catch (const std::logic_error& error) {
        failure = error.what();
    }

    if (failure.empty()) {
        std::cout << "acceptance_uid reserved=GREEN later_uid=" << later_uid << '\n';
    } else {
        std::cout << "acceptance_uid delayed_claim=RED later_uid=" << later_uid
                  << " error=" << failure << '\n';
    }
    EXPECT_TRUE(failure.empty());
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

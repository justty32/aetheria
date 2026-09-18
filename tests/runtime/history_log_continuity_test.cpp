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

[[nodiscard]] auto layer_fields(const aetheria::world::LayerCombatResult& value) {
    return std::tuple{
        value.layer, value.resolution_id, value.region_expected_loss_a,
        value.region_expected_loss_b, value.loss_a, value.loss_b,
        value.contribution_b.statistical_loss, value.contribution_b.delta_limit,
        value.contribution_b.requested_deviation,
        value.contribution_b.applied_deviation, value.contribution_b.final_loss,
        value.contribution_b.named_loss, value.contribution_b.unnamed_loss};
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

// history_log_multi_character_test.cpp：多角色槽的完整重放必須與冷存檔位元一致（A-1）。

#include "core/runtime/character_save.h"
#include "core/runtime/playable_session.h"
#include "core/runtime/save_raws.h"
#include "core/zone/file_zone_store.h"
#include "sim/world_hash.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/zone/zone_test_support.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using aetheria::runtime::CharacterState;
using aetheria::runtime::PlayableSession;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;

struct LoadedCharacter {
    std::unique_ptr<aetheria::rules::Ruleset> ruleset;
    std::unique_ptr<aetheria::zone::FileZoneStore> store;
    std::unique_ptr<PlayableSession> session;
};

[[nodiscard]] LoadedCharacter load_character(const std::filesystem::path& slot,
                                             std::string_view name) {
    LoadedCharacter loaded;
    loaded.ruleset = std::make_unique<aetheria::rules::Ruleset>(
        aetheria::rules::RulesetLoader::load(
            aetheria::runtime::save_raws_directory(slot)));
    loaded.store =
        std::make_unique<aetheria::zone::FileZoneStore>(slot, *loaded.ruleset);
    loaded.session = PlayableSession::load(slot, *loaded.store, name);
    return loaded;
}

// 與 character_save_persistence 的雙角色流程同構：A 深入 Local 後存檔，
// 另一個 B 從 Region 出發改動世界後存檔，再切回 A 存檔，最後只讀不存地載入 B。
// 曾經的缺陷：冷讀在 initialize_loaded_scenario 卸載覆蓋 Site 時直接寫盤，
// 那一筆改動不在日誌也不經 commit，重放因此對不上（A-1）。
TEST(HistoryMultiCharacter, TwoCharacterSlotFullReplayMatchesStoredWorld) {
    TemporaryDirectory workspace;
    const auto slot = workspace.path() / "world";
    {
        aetheria::zone::InMemoryZoneStore active_store{test_ruleset()};
        PlayableSession session{UINT64_C(515151), 51,
                                AETHERIA_SOURCE_DIR "/data", active_store};
        session.issue_move(session.player_army_id(), session.coverage_tile());
        for (std::size_t xun = 0; xun < 2; ++xun) {
            ASSERT_FALSE(session.advance_xun().encounter_pending);
        }
        session.enter_site();
        session.accept_bandit_quest();
        session.enter_local();
        session.open_door_and_move();
        aetheria::zone::FileZoneStore destination{slot, test_ruleset()};
        session.save_game(destination, "A");
        ASSERT_TRUE(destination.manifest().has_value());
        aetheria::runtime::write_character_save(
            aetheria::runtime::character_save_path(slot, "B"),
            {destination.manifest()->world_seed, destination.manifest()->raws_hash},
            CharacterState{aetheria::world::StableId{1001}, "region", 0, 31, 32,
                           std::nullopt});
    }
    {
        auto b = load_character(slot, "B");
        b.session->sign_peace_treaty();
        b.session->enter_site();
        b.session->enter_local();
        b.session->save_game(*b.store, "B");
    }
    {
        auto a = load_character(slot, "A");
        a.session->save_game(*a.store, "A");
    }
    // 只讀不存：冷讀不得在日誌與 commit 之外改寫世界槽。
    const auto before_bare_load = aetheria::sim::world_state_hash(slot);
    {
        auto b = load_character(slot, "B");
    }
    EXPECT_EQ(aetheria::sim::world_state_hash(slot).hash, before_bare_load.hash);

    const auto stored = aetheria::sim::world_state_hash(slot);
    int result = -1;
    std::string failure;
    try {
        result = aetheria::sim::run_replay(slot);
    } catch (const std::runtime_error& error) {
        failure = error.what();
    }
    EXPECT_EQ(failure, "");
    EXPECT_EQ(result, 0);
    std::cout << "multi_character stored_world_hash=" << stored.hash
              << " history_seq=" << stored.history_head_seq << '\n';
}

}  // namespace

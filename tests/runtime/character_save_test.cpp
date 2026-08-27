#include "core/runtime/character_save.h"
#include "core/runtime/playable_session.h"
#include "core/zone/file_zone_store.h"
#include "sim/world_hash.h"
#include "tests/sim/world_hash_test_support.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/zone/zone_test_support.h"

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

namespace {

using aetheria::runtime::CharacterState;
using aetheria::runtime::CharacterWorldIdentity;
using aetheria::runtime::PlayableSession;
using aetheria::tests::read_binary;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::tests::write_binary;

[[nodiscard]] constexpr CharacterState initial_character() {
    return {aetheria::world::StableId{1001}, "region", 0, 31, 32, std::nullopt};
}

void write_version_fixture(const std::filesystem::path& path,
                           std::uint32_t version,
                           CharacterWorldIdentity identity,
                           const CharacterState& state) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    cereal::PortableBinaryOutputArchive archive{
        stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
    archive(version, identity.world_seed, identity.raws_hash,
            state.player_army_id.uid, state.residence_id, state.local_z,
            state.local_player_x, state.local_player_y,
            state.accepted_quest_id);
    if (!stream) {
        throw std::runtime_error{"測試無法寫角色版本 fixture"};
    }
}

[[nodiscard]] std::unique_ptr<PlayableSession>
load_character(const std::filesystem::path& slot,
               std::unique_ptr<aetheria::rules::Ruleset>& ruleset,
               std::unique_ptr<aetheria::zone::FileZoneStore>& store,
               std::string_view name) {
    ruleset = std::make_unique<aetheria::rules::Ruleset>(
        aetheria::rules::RulesetLoader::load(
            aetheria::runtime::save_raws_directory(slot)));
    store = std::make_unique<aetheria::zone::FileZoneStore>(slot, *ruleset);
    return PlayableSession::load(slot, *store, name);
}

[[nodiscard]] bool demo_door_is_open(const PlayableSession& session) {
    const auto view = session.local_view();
    if (!view.has_value()) {
        return false;
    }
    constexpr std::size_t kDoorCell = 32U * 64U + 32U;
    return view->cells.at(kDoorCell) != UINT8_C(6);
}

TEST(CharacterSave, CodecIsDeterministicAndRejectsVersionOrWorldMismatch) {
    TemporaryDirectory directory;
    const auto path = directory.path() / "chars" / "A.bin";
    const CharacterWorldIdentity identity{515151, 987654321};
    const CharacterState state{aetheria::world::StableId{1001}, "dungeon", -3,
                               32, 32, UINT64_C(42)};

    aetheria::runtime::write_character_save(path, identity, state);
    const auto first = read_binary(path);
    EXPECT_EQ(aetheria::runtime::read_character_save(path, identity), state);
    aetheria::runtime::write_character_save(path, identity, state);
    EXPECT_EQ(read_binary(path), first);

    try {
        static_cast<void>(aetheria::runtime::read_character_save(
            path, {identity.world_seed, identity.raws_hash + 1U}));
        FAIL() << "raws hash mismatch should fail";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string{error.what()}.find("raws_hash"), std::string::npos);
    }

    write_version_fixture(path,
                          aetheria::runtime::kCharacterFormatVersion + 1U,
                          identity, state);
    try {
        static_cast<void>(aetheria::runtime::read_character_save(path, identity));
        FAIL() << "character version mismatch should fail";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string{error.what()}.find("character format_version 不符"),
                  std::string::npos);
    }
}

TEST(CharacterPersistence, TwoCharactersColdRoundTripAndWorldHashIsolation) {
    TemporaryDirectory workspace;
    const auto slot = workspace.path() / "world";
    CharacterState saved_a;
    std::string deterministic_a_bytes;

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
        saved_a = session.export_character_state();

        aetheria::zone::FileZoneStore destination{slot, test_ruleset()};
        session.save_game(destination, "A");
        const auto a_path = aetheria::runtime::character_save_path(slot, "A");
        deterministic_a_bytes = read_binary(a_path);
        session.save_game(destination, "A");
        EXPECT_EQ(read_binary(a_path), deterministic_a_bytes);

        ASSERT_TRUE(destination.manifest().has_value());
        aetheria::runtime::write_character_save(
            aetheria::runtime::character_save_path(slot, "B"),
            {destination.manifest()->world_seed,
             destination.manifest()->raws_hash},
            initial_character());
    }

    CharacterState saved_b;
    {
        std::unique_ptr<aetheria::rules::Ruleset> ruleset;
        std::unique_ptr<aetheria::zone::FileZoneStore> store;
        auto b = load_character(slot, ruleset, store, "B");
        b->sign_peace_treaty();
        b->enter_site();
        b->enter_local();
        EXPECT_TRUE(demo_door_is_open(*b));
        saved_b = b->export_character_state();
        EXPECT_NE(saved_b, saved_a);
        b->save_game(*store, "B");
    }

    {
        std::unique_ptr<aetheria::rules::Ruleset> ruleset;
        std::unique_ptr<aetheria::zone::FileZoneStore> store;
        auto a = load_character(slot, ruleset, store, "A");
        EXPECT_EQ(a->export_character_state(), saved_a);
        EXPECT_TRUE(demo_door_is_open(*a));
        a->save_game(*store, "A");
        EXPECT_EQ(read_binary(aetheria::runtime::character_save_path(slot, "A")),
                  deterministic_a_bytes);
    }

    {
        std::unique_ptr<aetheria::rules::Ruleset> ruleset;
        std::unique_ptr<aetheria::zone::FileZoneStore> store;
        auto b = load_character(slot, ruleset, store, "B");
        EXPECT_EQ(b->export_character_state(), saved_b);
        EXPECT_TRUE(demo_door_is_open(*b));
    }

    EXPECT_EQ(aetheria::runtime::list_character_saves(slot),
              (std::vector<std::string>{"A", "B"}));
    const auto a_path = aetheria::runtime::character_save_path(slot, "A");
    const auto b_path = aetheria::runtime::character_save_path(slot, "B");
    const auto a_bytes = read_binary(a_path);
    const auto b_bytes = read_binary(b_path);
    const auto hash_two = aetheria::sim::world_state_hash(slot);
    ASSERT_TRUE(std::filesystem::remove(b_path));
    const auto hash_one = aetheria::sim::world_state_hash(slot);
    ASSERT_TRUE(std::filesystem::remove(a_path));
    const auto hash_zero = aetheria::sim::world_state_hash(slot);
    EXPECT_EQ(hash_zero.hash, hash_one.hash);
    EXPECT_EQ(hash_one.hash, hash_two.hash);
    EXPECT_EQ(hash_zero.zone_count, hash_one.zone_count);
    EXPECT_EQ(hash_one.zone_count, hash_two.zone_count);
    std::cout << "characters=0 world_hash=" << hash_zero.hash
              << " zone_count=" << hash_zero.zone_count << '\n'
              << "characters=1 world_hash=" << hash_one.hash
              << " zone_count=" << hash_one.zone_count << '\n'
              << "characters=2 world_hash=" << hash_two.hash
              << " zone_count=" << hash_two.zone_count << '\n';
    write_binary(a_path, a_bytes);
    write_binary(b_path, b_bytes);

    ASSERT_TRUE(std::filesystem::remove(a_path));
    {
        aetheria::zone::FileZoneStore world_only{slot, test_ruleset()};
        EXPECT_TRUE(world_only.manifest().has_value());
    }
    {
        std::unique_ptr<aetheria::rules::Ruleset> ruleset;
        std::unique_ptr<aetheria::zone::FileZoneStore> store;
        auto b = load_character(slot, ruleset, store, "B");
        EXPECT_EQ(b->export_character_state(), saved_b);
    }
    write_binary(a_path, a_bytes);

    const auto other_slot = workspace.path() / "other-world";
    {
        aetheria::zone::InMemoryZoneStore active_store{test_ruleset()};
        PlayableSession other{UINT64_C(999999), 51,
                              AETHERIA_SOURCE_DIR "/data", active_store};
        aetheria::zone::FileZoneStore destination{other_slot, test_ruleset()};
        other.save_game(destination, "native");
    }
    std::filesystem::copy_file(
        a_path, aetheria::runtime::character_save_path(other_slot, "A"),
        std::filesystem::copy_options::overwrite_existing);
    try {
        std::unique_ptr<aetheria::rules::Ruleset> ruleset;
        std::unique_ptr<aetheria::zone::FileZoneStore> store;
        static_cast<void>(load_character(other_slot, ruleset, store, "A"));
        FAIL() << "cross-world character should fail";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string{error.what()}.find("world_seed 與世界不符"),
                  std::string::npos);
        std::cout << "cross_world rejected=1 error=" << error.what() << '\n';
    }

    if (const char* evidence = std::getenv("AETHERIA_M10_2_EVIDENCE_SLOT");
        evidence != nullptr && *evidence != '\0') {
        std::filesystem::copy(slot, std::filesystem::path{evidence},
                              std::filesystem::copy_options::recursive);
    }
}

}  // namespace

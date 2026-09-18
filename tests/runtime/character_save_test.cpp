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

}  // namespace

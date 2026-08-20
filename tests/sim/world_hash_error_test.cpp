#include "tests/sim/world_hash_test_support.h"

#include "core/serialize/zone_codec.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

namespace {

using aetheria::serialize::kSaveFormatVersion;
using aetheria::sim::world_state_hash;
using aetheria::tests::compress_for_test;
using aetheria::tests::create_world_hash_save;
using aetheria::tests::decompress_for_test;
using aetheria::tests::kWorldHashRegionKeys;
using aetheria::tests::read_binary;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::tests::write_binary;
using aetheria::zone::FileZoneStore;

void expect_error_contains(const std::filesystem::path& directory, std::string_view expected) {
    try {
        static_cast<void>(world_state_hash(directory, test_ruleset()));
        FAIL() << "world_state_hash should throw";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string{error.what()}.find(expected), std::string::npos) << error.what();
    }
}

TEST(WorldStateHashErrors, MissingDirectoryFailsWithPath) {
    TemporaryDirectory parent;
    const auto missing = parent.path() / "missing-slot";
    expect_error_contains(missing, missing.string());
}

TEST(WorldStateHashErrors, EmptyDirectoryFailsWithPath) {
    TemporaryDirectory empty;
    expect_error_contains(empty.path(), empty.path().string());
}

TEST(WorldStateHashErrors, CorruptZoneFailsWithFilePath) {
    TemporaryDirectory directory;
    create_world_hash_save(directory.path());
    FileZoneStore store{directory.path(), test_ruleset()};
    const auto path = store.path_for(kWorldHashRegionKeys.at(1));
    write_binary(path, "not-a-zstd-frame");
    expect_error_contains(directory.path(), path.string());
}

TEST(WorldStateHashErrors, VersionMismatchFailsWithFilePath) {
    TemporaryDirectory directory;
    create_world_hash_save(directory.path());
    FileZoneStore store{directory.path(), test_ruleset()};
    const auto path = store.path_for(kWorldHashRegionKeys.at(2));
    auto raw = decompress_for_test(read_binary(path));
    ASSERT_GE(raw.size(), 9U);
    const std::uint32_t bad_version = kSaveFormatVersion + 1U;
    for (std::size_t index = 0; index < sizeof(bad_version); ++index) {
        raw.at(5U + index) = static_cast<char>((bad_version >> (index * 8U)) & UINT32_C(0xFF));
    }
    write_binary(path, compress_for_test(raw));
    expect_error_contains(directory.path(), path.string());
}

}  // namespace

#include "core/serialize/zone_codec.h"
#include "core/zone/file_zone_store.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/zone/zone_test_support.h"

#include <zstd.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::serialize::encode_zone;
using aetheria::serialize::persistent_state_hash;
using aetheria::tests::entity_count;
using aetheria::tests::populated_zone;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::zone::child_key;
using aetheria::zone::FileZoneStore;
using aetheria::zone::kRootZone;
using aetheria::zone::value_of;
using aetheria::zone::ZoneKey;
using aetheria::zone::ZoneMeta;

[[nodiscard]] std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary | std::ios::ate};
    if (!stream) {
        throw std::runtime_error{"測試無法讀檔"};
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

TEST(FileZoneStore, RoundTripPreservesCanonicalBitsAndEntityCount) {
    TemporaryDirectory directory;
    FileZoneStore store{directory.path(), test_ruleset()};
    const auto key = child_key(kRootZone, UINT16_C(0xA3F2), 0);
    auto source = populated_zone(key);
    const auto before_hash = persistent_state_hash(source, test_ruleset());
    const auto before_entities = entity_count(source);

    store.save(source);
    const auto compressed = read_file(store.path_for(key));
    EXPECT_NE(ZSTD_getFrameContentSize(compressed.data(), compressed.size()),
              ZSTD_CONTENTSIZE_ERROR);
    const auto loaded = store.load(key);

    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(persistent_state_hash(*loaded, test_ruleset()), before_hash);
    EXPECT_EQ(encode_zone(*loaded, test_ruleset()), encode_zone(source, test_ruleset()));
    EXPECT_EQ(entity_count(*loaded), before_entities);
    EXPECT_EQ(loaded->reg.view<const ZoneMeta>().size(), 2U);
    const auto& loaded_layers = std::get<aetheria::zone::RegionPayload>(loaded->payload).layers;
    EXPECT_EQ(loaded_layers.at(0).moisture,
              (std::vector<std::uint8_t>{55, 66, 77, 88}));
    EXPECT_EQ(loaded_layers.at(0).settlement.at(1), aetheria::world::SettlementTier::Town);
    EXPECT_EQ(loaded_layers.at(0).site.at(0).lod, aetheria::zone::LodLevel::Absent);
    EXPECT_TRUE(loaded_layers.at(0).site.at(0).ever_realized);
    EXPECT_EQ(loaded_layers.at(-1).elevation,
              (std::vector<std::uint16_t>{99, 100, 101}));
}

TEST(FileZoneStore, DerivesStableBucketedPathsWithoutCollisions) {
    TemporaryDirectory directory;
    FileZoneStore store{directory.path(), test_ruleset()};
    const ZoneKey first{UINT64_C(0xA3F2000100200000)};
    const ZoneKey second{UINT64_C(0xA4F2000100200000)};

    EXPECT_EQ(store.path_for(first), store.path_for(first));
    EXPECT_NE(store.path_for(first), store.path_for(second));
    EXPECT_EQ(store.path_for(first).parent_path().filename(), "6f");
    EXPECT_EQ(store.path_for(first).filename(), "a3f2000100200000.bin");
    EXPECT_EQ(store.path_for(kRootZone).filename(), "root.bin");
}

TEST(FileZoneStore, SpreadsSameLevelCoordinatesAcrossMultipleBuckets) {
    TemporaryDirectory directory;
    FileZoneStore store{directory.path(), test_ruleset()};
    const auto region = child_key(kRootZone, 1, 0);
    const auto site = child_key(region, 7, 9);
    std::set<std::filesystem::path> buckets;

    for (std::uint32_t coordinate = 0; coordinate < 64; ++coordinate) {
        const auto local = child_key(site, coordinate, coordinate);
        buckets.insert(store.path_for(local).parent_path().filename());
    }

    EXPECT_GT(buckets.size(), 1U);
}

TEST(FileZoneStore, RejectsStoredKeyDifferentFromRequestedKeyWithBothValues) {
    TemporaryDirectory directory;
    FileZoneStore store{directory.path(), test_ruleset()};
    const auto requested = child_key(kRootZone, 1, 0);
    const auto stored = child_key(kRootZone, 2, 0);
    auto source = populated_zone(stored);
    store.save(source);
    std::filesystem::create_directories(store.path_for(requested).parent_path());
    std::filesystem::rename(store.path_for(stored), store.path_for(requested));

    try {
        static_cast<void>(store.load(requested));
        FAIL() << "key mismatch should throw";
    } catch (const std::runtime_error& exception) {
        const std::string message = exception.what();
        EXPECT_NE(message.find(std::to_string(value_of(requested))), std::string::npos);
        EXPECT_NE(message.find(std::to_string(value_of(stored))), std::string::npos);
    }
}

}  // namespace

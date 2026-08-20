#include "core/serialize/zone_codec.h"
#include "core/time/tick.h"
#include "core/worldgen/region_generator.h"
#include "core/zone/file_zone_store.h"
#include "core/zone/zone_manager.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/zone/zone_test_support.h"

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/array.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace {

using aetheria::serialize::kSaveFormatVersion;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::time::Tick;
using aetheria::zone::child_key;
using aetheria::zone::FileZoneStore;
using aetheria::zone::kRootZone;
using aetheria::zone::SaveManifest;
using aetheria::zone::ZoneManager;

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

void write_file(const std::filesystem::path& path, std::string_view bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw std::runtime_error{"測試無法寫檔"};
    }
}

void write_raw_manifest(const std::filesystem::path& path, const SaveManifest& manifest) {
    std::ostringstream stream{std::ios::binary};
    cereal::PortableBinaryOutputArchive archive{
        stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
    const auto now = static_cast<std::int64_t>(manifest.now);
    archive(manifest.format_version, manifest.next_detached_id, manifest.next_entity_uid,
            manifest.dims.region_width, manifest.dims.region_height, manifest.dims.site_width,
            manifest.dims.site_height, manifest.world_seed, manifest.generation_parameters.groups,
            now);
    write_file(path, std::move(stream).str());
}

TEST(FileZoneStore, ManifestRoundTripsAndAtomicallyReplaces) {
    TemporaryDirectory directory;
    SaveManifest expected;
    expected.next_detached_id = 45;
    expected.next_entity_uid = 67;
    expected.world_seed = 89;
    expected.now = Tick{1234};
    {
        FileZoneStore store{directory.path(), test_ruleset()};
        ZoneManager manager{store};
        manager.save_all();
        store.write_manifest(expected);
        expected.world_seed = 90;
        store.write_manifest(expected);
        EXPECT_FALSE(std::filesystem::exists(store.manifest_path().string() + ".tmp"));
    }
    {
        FileZoneStore store{directory.path(), test_ruleset()};
        ASSERT_TRUE(store.manifest().has_value());
        EXPECT_EQ(*store.manifest(), expected);
        ZoneManager manager{store};
        EXPECT_TRUE(manager.get(kRootZone).has_value());
    }
}

TEST(FileZoneStore, RejectsChangingImmutableWorldDims) {
    TemporaryDirectory directory;
    FileZoneStore store{directory.path(), test_ruleset()};
    ZoneManager manager{store};
    manager.save_all();
    store.write_manifest(SaveManifest{});
    auto changed = *store.manifest();
    ++changed.dims.region_width;

    EXPECT_THROW(store.write_manifest(changed), std::runtime_error);
    EXPECT_EQ(store.manifest()->dims, aetheria::zone::WorldDims{});
}

TEST(FileZoneStore, RejectsTruncatedManifestWithoutOverwritingExistingFiles) {
    TemporaryDirectory directory;
    std::filesystem::path root_path;
    std::filesystem::path zone_path;
    std::string root_before;
    std::string zone_before;
    {
        FileZoneStore store{directory.path(), test_ruleset()};
        ZoneManager manager{store};
        const auto key = child_key(kRootZone, 4, 0);
        static_cast<void>(manager.materialize(key));
        manager.save_all();
        store.write_manifest(SaveManifest{});
        root_path = store.path_for(kRootZone);
        zone_path = store.path_for(key);
        root_before = read_file(root_path);
        zone_before = read_file(zone_path);
        const auto manifest = read_file(store.manifest_path());
        ASSERT_GT(manifest.size(), 3U);
        write_file(store.manifest_path(), std::string_view{manifest}.substr(0, 3));
    }

    EXPECT_THROW(static_cast<void>(FileZoneStore{directory.path(), test_ruleset()}),
                 std::runtime_error);
    EXPECT_EQ(read_file(root_path), root_before);
    EXPECT_EQ(read_file(zone_path), zone_before);
    EXPECT_EQ(std::filesystem::file_size(directory.path() / "manifest.bin"), 3U);
}

TEST(FileZoneStore, RejectsZoneFilesWithoutManifestAndDoesNotOverwriteThem) {
    TemporaryDirectory directory;
    const auto orphan = directory.path() / "ab" / "abcdef.bin";
    write_file(orphan, "orphan-data");

    EXPECT_THROW(static_cast<void>(FileZoneStore{directory.path(), test_ruleset()}),
                 std::runtime_error);
    EXPECT_EQ(read_file(orphan), "orphan-data");
    EXPECT_FALSE(std::filesystem::exists(directory.path() / "manifest.bin"));
}

TEST(FileZoneStore, RejectsManifestFormatVersionMismatch) {
    TemporaryDirectory directory;
    SaveManifest bad;
    bad.format_version = kSaveFormatVersion + 1;
    write_raw_manifest(directory.path() / "manifest.bin", bad);

    EXPECT_THROW(static_cast<void>(FileZoneStore{directory.path(), test_ruleset()}),
                 std::runtime_error);
    EXPECT_EQ(read_file(directory.path() / "manifest.bin").size(), 133U);
}

TEST(FileZoneStore, RejectsGenerationParameterGroupMismatchByName) {
    TemporaryDirectory directory;
    {
        FileZoneStore store{directory.path(), test_ruleset()};
        ZoneManager manager{store};
        manager.save_all();
        store.write_manifest(SaveManifest{});
    }
    aetheria::worldgen::RegionGenerationConfig changed;
    ++changed.erosion.iterations;

    try {
        static_cast<void>(FileZoneStore{directory.path(), test_ruleset(),
                                        aetheria::worldgen::generation_parameter_hashes(changed)});
        FAIL() << "generation parameter mismatch should throw";
    } catch (const std::runtime_error& error) {
        std::cout << "parameter_mismatch_error=" << error.what() << '\n';
        EXPECT_NE(std::string{error.what()}.find("erosion"), std::string::npos);
    }
}

TEST(FileZoneStore, RequiresRootWhenManifestExists) {
    TemporaryDirectory directory;
    {
        FileZoneStore store{directory.path(), test_ruleset()};
        ZoneManager manager{store};
        manager.save_all();
        store.write_manifest(SaveManifest{});
        ASSERT_TRUE(std::filesystem::remove(store.path_for(kRootZone)));
    }
    EXPECT_THROW(static_cast<void>(FileZoneStore{directory.path(), test_ruleset()}),
                 std::runtime_error);
}

TEST(FileZoneStore, ManagerDestroySynchronouslyDeletesFile) {
    TemporaryDirectory directory;
    FileZoneStore store{directory.path(), test_ruleset()};
    ZoneManager manager{store};
    const auto key = child_key(kRootZone, 5, 0);
    static_cast<void>(manager.materialize(key));
    manager.tick(Tick{77});
    manager.save_all();
    ASSERT_TRUE(store.contains(key));

    EXPECT_TRUE(manager.destroy(key));
    EXPECT_FALSE(store.contains(key));
    EXPECT_FALSE(manager.load(key));
}

}  // namespace

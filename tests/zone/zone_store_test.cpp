#include "core/serialize/all_components.h"
#include "core/serialize/zone_codec.h"
#include "core/zone/file_zone_store.h"
#include "core/zone/zone_manager.h"
#include "tests/support/ruleset_fixture.h"

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/array.hpp>

#include <zstd.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::serialize::AllComponents;
using aetheria::serialize::decode_zone;
using aetheria::serialize::encode_zone;
using aetheria::serialize::kSaveFormatVersion;
using aetheria::serialize::persistent_state_hash;
using aetheria::tests::test_ruleset;
using aetheria::time::Tick;
using aetheria::zone::child_key;
using aetheria::zone::FileZoneStore;
using aetheria::zone::InMemoryZoneStore;
using aetheria::zone::kRootZone;
using aetheria::zone::SaveManifest;
using aetheria::zone::value_of;
using aetheria::zone::Zone;
using aetheria::zone::ZoneKey;
using aetheria::zone::ZoneManager;
using aetheria::zone::ZoneMeta;
using aetheria::zone::ZoneStore;

static_assert(AllComponents::size == 6);
static_assert(std::same_as<entt::type_list_element_t<0, AllComponents>, ZoneMeta>);

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        static std::atomic_uint64_t sequence{};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("aetheria-zone-store-" + std::to_string(stamp) + "-" +
                 std::to_string(sequence.fetch_add(1)));
        if (!std::filesystem::create_directories(path_)) {
            throw std::runtime_error{"無法建立測試暫存目錄"};
        }
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

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

[[nodiscard]] std::size_t entity_count(const Zone& zone) {
    const auto* entities = zone.reg.storage<entt::entity>();
    return entities == nullptr ? 0 : entities->free_list();
}

[[nodiscard]] Zone populated_zone(ZoneKey key) {
    Zone zone{key};
    zone.last_saved_tick = Tick{987'654'321};
    auto& layers = std::get<aetheria::zone::RegionPayload>(zone.payload).layers;
    auto& surface = layers.emplace(0, aetheria::world::RegionTiles{2, 2}).first->second;
    surface.temperature = {11, 22, 33, 44};
    surface.moisture = {55, 66, 77, 88};
    surface.settlement.at(1) = aetheria::world::SettlementTier::Town;
    surface.site.at(0).lod = aetheria::zone::LodLevel::Full;
    surface.site.at(0).ever_realized = true;
    auto& underground = layers.emplace(-1, aetheria::world::RegionTiles{3, 1}).first->second;
    underground.elevation = {99, 100, 101};
    const auto extra = zone.reg.create();
    zone.reg.emplace<ZoneMeta>(extra, value_of(key));
    return zone;
}

void expect_store_contract(ZoneStore& store) {
    const auto& ruleset = test_ruleset();
    const auto key = child_key(kRootZone, UINT16_C(0x1234), 0);
    auto source = populated_zone(key);
    const auto source_hash = persistent_state_hash(source, ruleset);
    const auto source_entities = entity_count(source);

    EXPECT_FALSE(store.contains(key));
    EXPECT_EQ(store.load(key), nullptr);
    store.save(source);
    EXPECT_TRUE(store.contains(key));

    const auto first = store.load(key);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(persistent_state_hash(*first, ruleset), source_hash);
    EXPECT_EQ(entity_count(*first), source_entities);
    EXPECT_EQ(encode_zone(*first, ruleset), encode_zone(source, ruleset));

    const auto second = store.load(key);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(persistent_state_hash(*second, ruleset), source_hash);
    EXPECT_TRUE(store.contains(key));

    EXPECT_TRUE(store.erase(key));
    EXPECT_FALSE(store.contains(key));
    EXPECT_EQ(store.load(key), nullptr);
    EXPECT_FALSE(store.erase(key));
}

[[nodiscard]] std::string zstd_decompress(std::string_view compressed) {
    const auto size = ZSTD_getFrameContentSize(compressed.data(), compressed.size());
    if (size == ZSTD_CONTENTSIZE_ERROR || size == ZSTD_CONTENTSIZE_UNKNOWN) {
        throw std::runtime_error{"測試輸入不是已知大小的 zstd frame"};
    }
    std::string output(static_cast<std::size_t>(size), '\0');
    const auto written =
        ZSTD_decompress(output.data(), output.size(), compressed.data(), compressed.size());
    if (ZSTD_isError(written) != 0 || written != output.size()) {
        throw std::runtime_error{"測試解壓失敗"};
    }
    return output;
}

[[nodiscard]] std::string zstd_compress(std::string_view input) {
    std::string output(ZSTD_compressBound(input.size()), '\0');
    const auto written = ZSTD_compress(output.data(), output.size(), input.data(), input.size(), 3);
    if (ZSTD_isError(written) != 0) {
        throw std::runtime_error{"測試壓縮失敗"};
    }
    output.resize(written);
    return output;
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

TEST(ZoneStoreContract, InMemoryBackend) {
    InMemoryZoneStore store{test_ruleset()};
    expect_store_contract(store);
}

TEST(ZoneStoreContract, FileBackend) {
    TemporaryDirectory directory;
    FileZoneStore store{directory.path(), test_ruleset()};
    expect_store_contract(store);
}

TEST(ZonePersistence, NewZoneStartsWithMatchingPlaceholder) {
    const auto key = child_key(kRootZone, UINT16_C(0x4321), 0);
    const Zone zone{key};
    const auto meta = zone.reg.view<const ZoneMeta>();

    ASSERT_EQ(meta.size(), 1U);
    EXPECT_EQ(meta.get<ZoneMeta>(*meta.begin()).zone_key, value_of(key));
    EXPECT_EQ(entity_count(zone), 1U);
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

TEST(FileZoneStore, RejectsZoneFormatVersionMismatch) {
    TemporaryDirectory directory;
    FileZoneStore store{directory.path(), test_ruleset()};
    const auto key = child_key(kRootZone, 3, 0);
    auto source = populated_zone(key);
    store.save(source);

    auto raw = zstd_decompress(read_file(store.path_for(key)));
    ASSERT_GE(raw.size(), 9U);
    const std::uint32_t bad_version = kSaveFormatVersion + 1;
    for (std::size_t index = 0; index < sizeof(bad_version); ++index) {
        raw[5 + index] = static_cast<char>((bad_version >> (index * 8U)) & UINT32_C(0xFF));
    }
    write_file(store.path_for(key), zstd_compress(raw));

    EXPECT_THROW(static_cast<void>(store.load(key)), std::runtime_error);
}

TEST(ZoneCodec, RejectsPayloadAlternativeMismatchDuringDecode) {
    const auto region = child_key(kRootZone, 3, 0);
    const auto site = child_key(region, 4, 5);
    auto source = populated_zone(region);
    auto bytes = encode_zone(source, test_ruleset());
    ASSERT_GE(bytes.size(), 17U);
    const auto site_bits = value_of(site);
    for (std::size_t index = 0; index < sizeof(site_bits); ++index) {
        bytes[9 + index] = static_cast<char>((site_bits >> (index * 8U)) & UINT64_C(0xFF));
    }

    try {
        static_cast<void>(decode_zone(bytes, test_ruleset()));
        FAIL() << "payload/key mismatch should throw";
    } catch (const std::invalid_argument& error) {
        EXPECT_NE(std::string{error.what()}.find("SpatialPayload"), std::string::npos);
    }
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
    EXPECT_EQ(read_file(directory.path() / "manifest.bin").size(), 125U);
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

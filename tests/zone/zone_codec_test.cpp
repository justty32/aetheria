#include "core/serialize/all_components.h"
#include "core/serialize/zone_codec.h"
#include "core/zone/file_zone_store.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/zone/zone_test_support.h"

#include <zstd.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace {

using aetheria::serialize::AllComponents;
using aetheria::serialize::decode_zone;
using aetheria::serialize::encode_zone;
using aetheria::serialize::kSaveFormatVersion;
using aetheria::tests::populated_zone;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::zone::child_key;
using aetheria::zone::FileZoneStore;
using aetheria::zone::kRootZone;
using aetheria::zone::value_of;
using aetheria::zone::ZoneMeta;

static_assert(AllComponents::size == 9);
static_assert(std::same_as<entt::type_list_element_t<0, AllComponents>, ZoneMeta>);

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

TEST(FileZoneStore, RejectsZoneFormatVersionMismatch) {
    TemporaryDirectory directory;
    FileZoneStore store{directory.path(), test_ruleset()};
    const auto key = child_key(kRootZone, 3, 0);
    auto source = populated_zone(key);
    store.save(source);

    auto raw = zstd_decompress(read_file(store.path_for(key)));
    ASSERT_GE(raw.size(), 9U);
    static_assert(kSaveFormatVersion == 20);
    const std::uint32_t bad_version = 19;
    for (std::size_t index = 0; index < sizeof(bad_version); ++index) {
        raw[5 + index] = static_cast<char>((bad_version >> (index * 8U)) & UINT32_C(0xFF));
    }
    write_file(store.path_for(key), zstd_compress(raw));

    try {
        static_cast<void>(store.load(key));
        FAIL() << "v19 zone should be rejected by v20 decoder";
    } catch (const std::runtime_error& error) {
        std::cout << "zone_v17_reject_error=" << error.what() << '\n';
        EXPECT_NE(std::string{error.what()}.find("檔內=19 預期=20"), std::string::npos);
    }
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

}  // namespace

#pragma once

#include "core/serialize/zone_codec.h"
#include "core/world/region_movement.h"
#include "core/zone/file_zone_store.h"
#include "sim/world_hash.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/world/region_test_support.h"
#include "tests/zone/zone_test_support.h"

#include <zstd.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace aetheria::tests {

inline constexpr std::array kWorldHashRegionKeys{zone::child_key(zone::kRootZone, 7, 0),
                                                 zone::child_key(zone::kRootZone, 2, 0),
                                                 zone::child_key(zone::kRootZone, 13, 0)};

[[nodiscard]] inline std::string read_binary(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary | std::ios::ate};
    if (!stream) {
        throw std::runtime_error{"測試無法讀檔：" + path.string()};
    }
    const auto end = stream.tellg();
    if (end < 0) {
        throw std::runtime_error{"測試無法取得檔案大小：" + path.string()};
    }
    std::string bytes(static_cast<std::size_t>(end), '\0');
    stream.seekg(0);
    if (!bytes.empty()) {
        stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    if (!stream) {
        throw std::runtime_error{"測試無法完整讀檔：" + path.string()};
    }
    return bytes;
}

inline void write_binary(const std::filesystem::path& path, std::string_view bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw std::runtime_error{"測試無法寫檔：" + path.string()};
    }
}

[[nodiscard]] inline std::unique_ptr<zone::Zone> world_hash_region(zone::ZoneKey key,
                                                                   bool reverse_units) {
    auto result = std::make_unique<zone::Zone>(key);
    std::get<zone::RegionPayload>(result->payload)
        .layers.emplace(0, plain_tiles(12, 1, test_ruleset()));
    result->reg.emplace<world::TurnClock>(placeholder(*result), time::Tick{123'456});
    if (reverse_units) {
        add_unit(*result, world::StableId{20}, world::RegionXY{11, 0}, world::RegionXY{0, 0});
        add_unit(*result, world::StableId{10}, world::RegionXY{0, 0}, world::RegionXY{11, 0});
    } else {
        add_unit(*result, world::StableId{10}, world::RegionXY{0, 0}, world::RegionXY{11, 0});
        add_unit(*result, world::StableId{20}, world::RegionXY{11, 0}, world::RegionXY{0, 0});
    }
    return result;
}

inline void create_world_hash_save(const std::filesystem::path& directory,
                                   bool reverse_history = false) {
    zone::FileZoneStore store{directory, test_ruleset()};
    const zone::Zone root{zone::kRootZone};
    store.save(root);
    if (reverse_history) {
        for (auto iterator = kWorldHashRegionKeys.rbegin(); iterator != kWorldHashRegionKeys.rend();
             ++iterator) {
            store.save(*world_hash_region(*iterator, true));
        }
    } else {
        for (const auto key : kWorldHashRegionKeys) {
            store.save(*world_hash_region(key, false));
        }
    }
    store.write_manifest(zone::SaveManifest{});
}

[[nodiscard]] inline std::pair<std::uint64_t, std::size_t>
saved_zone_bytes_evidence(const std::filesystem::path& directory) {
    zone::FileZoneStore store{directory, test_ruleset()};
    auto hash = UINT64_C(14695981039346656037);
    std::size_t byte_count{};
    for (const auto key : kWorldHashRegionKeys) {
        const auto bytes = read_binary(store.path_for(key));
        byte_count += bytes.size();
        for (const auto byte : bytes) {
            hash ^= static_cast<unsigned char>(byte);
            hash *= UINT64_C(1099511628211);
        }
    }
    return {hash, byte_count};
}

[[nodiscard]] inline std::string decompress_for_test(std::string_view compressed) {
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

[[nodiscard]] inline std::string compress_for_test(std::string_view input) {
    std::string output(ZSTD_compressBound(input.size()), '\0');
    const auto written = ZSTD_compress(output.data(), output.size(), input.data(), input.size(), 3);
    if (ZSTD_isError(written) != 0) {
        throw std::runtime_error{"測試壓縮失敗"};
    }
    output.resize(written);
    return output;
}

}  // namespace aetheria::tests

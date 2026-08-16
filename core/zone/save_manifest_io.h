#pragma once

// FileZoneStore 用的底層檔案 I/O 與 manifest 序列化 helper。
// 原本是 file_zone_store.cpp 匿名 namespace 的內容，抽出以維持單檔案大小。

#include "core/zone/file_zone_store.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace aetheria::zone::detail {

[[nodiscard]] std::string read_file(const std::filesystem::path& path);

void atomic_replace(const std::filesystem::path& path, std::string_view bytes);

[[nodiscard]] std::string compress(std::string_view input);

[[nodiscard]] std::string decompress(std::string_view input);

[[nodiscard]] std::string encode_manifest(const SaveManifest& manifest);

[[nodiscard]] SaveManifest decode_manifest(std::string_view bytes);

[[nodiscard]] bool has_bin_without_manifest(const std::filesystem::path& directory);

[[nodiscard]] constexpr std::uint64_t mix_zone_key(std::uint64_t value) noexcept {
    value = (value ^ (value >> 30U)) * UINT64_C(0xBF58476D1CE4E5B9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94D049BB133111EB);
    return value ^ (value >> 31U);
}

void require_generation_parameters(const worldgen::GenerationParameterHashes& actual,
                                   const worldgen::GenerationParameterHashes& expected,
                                   std::string_view action);

}  // namespace aetheria::zone::detail

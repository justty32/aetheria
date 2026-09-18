#pragma once

#include "core/history/history_log.h"
#include "core/runtime/playable_session.h"
#include "core/runtime/save_raws.h"
#include "core/runtime/session_persistence.h"
#include "core/zone/file_zone_store.h"
#include "core/zone/save_manifest_io.h"
#include "sim/world_hash.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/zone/zone_test_support.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>

namespace aetheria::tests::history_log {

using aetheria::runtime::PlayableBattleChoice;
using aetheria::runtime::PlayableSession;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;

[[nodiscard]] inline std::string read_bytes(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary | std::ios::ate};
    if (!stream) {
        throw std::runtime_error{"測試無法讀檔：" + path.string()};
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

inline void write_bytes(const std::filesystem::path& path, std::string_view bytes) {
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw std::runtime_error{"測試無法寫檔"};
    }
}

inline void fold_u64_for_test(std::uint64_t& hash, std::uint64_t value) noexcept {
    constexpr auto kFnvPrime = UINT64_C(1099511628211);
    for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
        hash ^= static_cast<std::uint8_t>(value & UINT8_MAX);
        hash *= kFnvPrime;
        value >>= 8U;
    }
}

[[nodiscard]] inline std::uint64_t fold_identity_for_test(
    std::uint64_t zone_hash, std::uint64_t raws_hash,
    std::uint64_t head_hash) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    fold_u64_for_test(hash, zone_hash);
    fold_u64_for_test(hash, raws_hash);
    fold_u64_for_test(hash, head_hash);
    return hash;
}

}  // namespace aetheria::tests::history_log

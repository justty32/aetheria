#include "sim/world_hash.h"

#include "core/serialize/normalized_state_hash.h"
#include "core/zone/file_zone_store.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace aetheria::sim {
namespace {

inline constexpr std::uint64_t kFnvOffset = UINT64_C(14695981039346656037);
inline constexpr std::uint64_t kFnvPrime = UINT64_C(1099511628211);

void hash_u64(std::uint64_t& hash, std::uint64_t value) noexcept {
    for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
        hash ^= static_cast<std::uint8_t>(value & UINT8_MAX);
        hash *= kFnvPrime;
        value >>= 8U;
    }
}

[[nodiscard]] zone::ZoneKey key_from_path(const std::filesystem::path& path) {
    if (path.filename() == "root.bin") {
        return zone::kRootZone;
    }
    const auto stem = path.stem().string();
    std::uint64_t value{};
    const auto [end, error] = std::from_chars(stem.data(), stem.data() + stem.size(), value, 16);
    if (stem.size() != 16U || error != std::errc{} || end != stem.data() + stem.size()) {
        throw std::runtime_error{"無法從 zone 檔名解析 ZoneKey：" + path.string()};
    }
    return zone::ZoneKey{value};
}

[[nodiscard]] std::vector<std::filesystem::path>
find_zone_files(const std::filesystem::path& slot_directory) {
    std::error_code error;
    if (!std::filesystem::exists(slot_directory, error)) {
        if (error) {
            throw std::runtime_error{"無法檢查存檔目錄：" + slot_directory.string() + "：" +
                                     error.message()};
        }
        throw std::runtime_error{"存檔目錄不存在：" + slot_directory.string()};
    }
    if (!std::filesystem::is_directory(slot_directory, error) || error) {
        throw std::runtime_error{"存檔路徑不是可讀目錄：" + slot_directory.string()};
    }

    std::vector<std::filesystem::path> files;
    const auto manifest_path = (slot_directory / "manifest.bin").lexically_normal();
    for (std::filesystem::recursive_directory_iterator iterator{slot_directory, error}, end;
         iterator != end; iterator.increment(error)) {
        if (error) {
            throw std::runtime_error{"無法掃描存檔目錄：" + iterator->path().string() + "：" +
                                     error.message()};
        }
        const bool regular = iterator->is_regular_file(error);
        if (error) {
            throw std::runtime_error{"無法檢查存檔項目：" + iterator->path().string() + "：" +
                                     error.message()};
        }
        if (regular && iterator->path().extension() == ".bin" &&
            iterator->path().lexically_normal() != manifest_path) {
            files.push_back(iterator->path());
        }
    }
    if (error) {
        throw std::runtime_error{"無法掃描存檔目錄：" + slot_directory.string() + "：" +
                                 error.message()};
    }
    if (files.empty()) {
        throw std::runtime_error{"存檔目錄沒有 zone .bin：" + slot_directory.string()};
    }
    return files;
}

[[nodiscard]] std::unique_ptr<zone::FileZoneStore>
open_store(const std::filesystem::path& slot_directory, const rules::Ruleset& ruleset) {
    try {
        return std::make_unique<zone::FileZoneStore>(slot_directory, ruleset);
    } catch (const std::exception& exception) {
        throw std::runtime_error{"世界狀態雜湊無法開啟 " +
                                 (slot_directory / "manifest.bin").string() + "：" +
                                 exception.what()};
    }
}

}  // namespace

WorldStateHashReport world_state_hash(const std::filesystem::path& slot_directory,
                                      const rules::Ruleset& ruleset) {
    auto files = find_zone_files(slot_directory);
    auto store = open_store(slot_directory, ruleset);
    std::vector<std::pair<zone::ZoneKey, std::uint64_t>> zone_hashes;
    zone_hashes.reserve(files.size());
    for (const auto& path : files) {
        try {
            const auto key = key_from_path(path);
            if (path.lexically_normal() != store->path_for(key).lexically_normal()) {
                throw std::runtime_error{"zone 檔不在 ZoneKey 推導出的 canonical 路徑"};
            }
            auto loaded = store->load(key);
            if (loaded == nullptr) {
                throw std::runtime_error{"列舉到的 zone 檔無法載入"};
            }
            zone_hashes.emplace_back(key, serialize::normalized_state_hash(*loaded, ruleset));
        } catch (const std::exception& exception) {
            throw std::runtime_error{"世界狀態雜湊失敗：" + path.string() + "：" +
                                     exception.what()};
        }
    }

    std::ranges::sort(zone_hashes, {}, &std::pair<zone::ZoneKey, std::uint64_t>::first);
    for (std::size_t index = 1; index < zone_hashes.size(); ++index) {
        if (zone_hashes[index - 1].first == zone_hashes[index].first) {
            throw std::runtime_error{"世界狀態雜湊遇到重複 ZoneKey：" +
                                     std::to_string(zone::value_of(zone_hashes[index].first))};
        }
    }

    auto hash = kFnvOffset;
    hash_u64(hash, static_cast<std::uint64_t>(zone_hashes.size()));
    for (const auto& [key, zone_hash] : zone_hashes) {
        hash_u64(hash, zone::value_of(key));
        hash_u64(hash, zone_hash);
    }
    return {hash, zone_hashes.size()};
}

int run_world_hash(const std::filesystem::path& slot_directory, const rules::Ruleset& ruleset) {
    const auto start = std::chrono::steady_clock::now();
    const auto report = world_state_hash(slot_directory, ruleset);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const auto milliseconds = std::chrono::duration<double, std::milli>{elapsed}.count();
    std::cout << "world_hash=" << report.hash << " zone_count=" << report.zone_count
              << " elapsed_ms=" << milliseconds << '\n';
    return 0;
}

}  // namespace aetheria::sim

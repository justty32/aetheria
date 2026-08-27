#include "sim/world_hash.h"

#include "core/history/history_log.h"
#include "core/runtime/save_raws.h"
#include "core/runtime/playable_session.h"
#include "core/runtime/session_persistence.h"
#include "core/rules/ruleset.h"
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
        const auto relative = iterator->path().lexically_relative(slot_directory);
        if (!relative.empty() && *relative.begin() == "chars") {
            if (iterator->is_directory(error) && !error) {
                iterator.disable_recursion_pending();
            }
            if (error) {
                throw std::runtime_error{"無法檢查存檔項目：" +
                                         iterator->path().string() + "：" +
                                         error.message()};
            }
            continue;
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
        auto store = std::make_unique<zone::FileZoneStore>(slot_directory, ruleset);
        if (!store->manifest().has_value() || store->manifest()->raws_hash == 0) {
            throw std::runtime_error{"manifest 缺少 v23 raws 內容雜湊"};
        }
        return store;
    } catch (const std::exception& exception) {
        throw std::runtime_error{"世界狀態雜湊無法開啟 " +
                                 (slot_directory / "manifest.bin").string() + "：" +
                                 exception.what()};
    }
}

}  // namespace

std::uint64_t compose_world_identity(std::uint64_t zone_hash,
                                     std::uint64_t raws_hash,
                                     std::uint64_t history_head_hash) noexcept {
    auto hash = kFnvOffset;
    hash_u64(hash, zone_hash);
    hash_u64(hash, raws_hash);
    hash_u64(hash, history_head_hash);
    return hash;
}

WorldStateHashReport world_state_hash(const std::filesystem::path& slot_directory) {
    const auto raws_hash = runtime::save_raws_hash(slot_directory);
    const auto ruleset = rules::RulesetLoader::load(runtime::save_raws_directory(slot_directory));
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

    auto zone_hash = kFnvOffset;
    hash_u64(zone_hash, static_cast<std::uint64_t>(zone_hashes.size()));
    for (const auto& [key, component_hash] : zone_hashes) {
        hash_u64(zone_hash, zone::value_of(key));
        hash_u64(zone_hash, component_hash);
    }
    const history::HistoryLog history{slot_directory / "history.log"};
    return {
        .hash = compose_world_identity(zone_hash, raws_hash, history.head_hash()),
        .zone_count = zone_hashes.size(),
        .zone_hash = zone_hash,
        .raws_hash = raws_hash,
        .history_head_hash = history.head_hash(),
        .history_head_seq = history.head_seq(),
    };
}

int run_world_hash(const std::filesystem::path& slot_directory) {
    const auto start = std::chrono::steady_clock::now();
    const auto report = world_state_hash(slot_directory);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const auto milliseconds = std::chrono::duration<double, std::milli>{elapsed}.count();
    std::cout << "zone_hash=" << report.zone_hash
              << " zone_count=" << report.zone_count << '\n'
              << "raws_hash=" << report.raws_hash << '\n'
              << "history_head_hash=" << report.history_head_hash
              << " history_seq=" << report.history_head_seq << '\n'
              << "world_hash=" << report.hash << " elapsed_ms=" << milliseconds
              << '\n';
    return 0;
}

int run_replay(const std::filesystem::path& slot_directory) {
    const auto ruleset = rules::RulesetLoader::load(
        runtime::save_raws_directory(slot_directory));
    zone::FileZoneStore source{slot_directory, ruleset};
    const auto metadata = runtime::inspect_session_save(source);
    const history::HistoryLog source_history{slot_directory / "history.log"};

    zone::InMemoryZoneStore replay_store{ruleset};
    runtime::PlayableSession replay{metadata.world_seed, metadata.region_id,
                                    runtime::save_raws_directory(slot_directory).string(),
                                    replay_store};
    replay.replay_history_from(slot_directory);

    const auto unique = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto temporary = std::filesystem::temp_directory_path() /
                           ("aetheria-replay-" + unique);
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    } cleanup{temporary};
    zone::FileZoneStore replay_destination{temporary, ruleset};
    replay.save_game(replay_destination, "replay");

    const auto stored = world_state_hash(slot_directory);
    const auto replayed = world_state_hash(temporary);
    const auto replay_hash = compose_world_identity(
        replayed.zone_hash, replayed.raws_hash, source_history.head_hash());
    std::cout << "stored_zone_hash=" << stored.zone_hash
              << " replay_zone_hash=" << replayed.zone_hash << '\n'
              << "stored_raws_hash=" << stored.raws_hash
              << " replay_raws_hash=" << replayed.raws_hash << '\n'
              << "stored_head_hash=" << stored.history_head_hash
              << " replay_head_hash=" << source_history.head_hash() << '\n'
              << "stored_world_hash=" << stored.hash
              << " replay_world_hash=" << replay_hash
              << " history_seq=" << source_history.head_seq() << '\n';
    if (stored.zone_hash != replayed.zone_hash) {
        throw std::runtime_error{"sim replay zone 分量不一致"};
    }
    if (stored.raws_hash != replayed.raws_hash) {
        throw std::runtime_error{"sim replay raws 分量不一致"};
    }
    if (stored.history_head_hash != source_history.head_hash()) {
        throw std::runtime_error{"sim replay history head 分量不一致"};
    }
    if (stored.hash != replay_hash) {
        throw std::runtime_error{"sim replay 世界身分合成值不一致"};
    }
    return 0;
}

}  // namespace aetheria::sim

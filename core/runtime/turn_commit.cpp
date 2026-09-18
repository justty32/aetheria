#include "core/runtime/turn_commit.h"

#include "core/runtime/playable_session.h"
#include "core/runtime/session_persistence.h"
#include "core/zone/zone_manager.h"
#include "core/zone/zone_store.h"

#include <array>
#include <charconv>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>

namespace aetheria::runtime {

namespace {

[[nodiscard]] std::string marker_bytes(std::uint64_t value) {
    std::string bytes(sizeof(value), '\0');
    for (auto& byte : bytes) {
        byte = static_cast<char>(value & UINT8_MAX);
        value >>= 8U;
    }
    return bytes;
}

[[nodiscard]] std::uint64_t decode_marker(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    std::array<unsigned char, sizeof(std::uint64_t)> bytes{};
    stream.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    if (!stream || stream.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error{"history.commit 不是單一 u64：" + path.string()};
    }
    std::uint64_t value{};
    for (std::size_t index = bytes.size(); index-- > 0;) {
        value <<= 8U;
        value |= bytes[index];
    }
    return value;
}

void atomic_write(const std::filesystem::path& path, std::string_view bytes) {
    std::filesystem::create_directories(path.parent_path());
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream stream{temporary, std::ios::binary | std::ios::trunc};
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        stream.flush();
        if (!stream) {
            throw std::runtime_error{"history.commit 暫存寫入失敗"};
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        throw std::runtime_error{"history.commit 原子替換失敗：" + error.message()};
    }
}

} // namespace

TurnCommit::TurnCommit(std::filesystem::path slot_directory)
    : history_{log_path(slot_directory)},
      slot_directory_{std::move(slot_directory)} {
    load_marker();
}

const history::HistoryEntry& TurnCommit::record(time::Tick tick,
                                                std::string_view kind,
                                                std::string_view payload) {
    const auto& entry = history_.append(tick, kind, payload);
    if (!slot_directory_.empty()) {
        ensure_marker();
    }
    if (interrupt_after_journal_) {
        throw std::runtime_error{"M10.3a 測試鉤：日誌與 marker 已落，套用未跑"};
    }
    return entry;
}

void TurnCommit::attach(const std::filesystem::path& slot_directory) {
    if (!slot_directory_.empty() &&
        slot_directory_.lexically_normal() != slot_directory.lexically_normal()) {
        throw std::logic_error{"turn commit 已綁定另一個世界槽"};
    }
    history_.bind(log_path(slot_directory));
    slot_directory_ = slot_directory;
    load_marker();
}

void TurnCommit::commit_world(zone::ZoneStore& active_store,
                              zone::ZoneStore& destination,
                              zone::ZoneManager& manager,
                              std::uint64_t world_seed,
                              std::uint64_t raws_hash, time::Tick now,
                              const std::function<void()>& persist_before_marker) {
    save_session(active_store, destination, manager, world_seed, raws_hash, now);
    if (persist_before_marker) {
        persist_before_marker();
    }
    if (interrupt_after_save_) {
        throw std::runtime_error{
            "M10.3a 測試鉤：zone/manifest 與角色檔已落，marker 未前推"};
    }
    write_marker(history_.head_seq());
}

std::filesystem::path TurnCommit::log_path(
    const std::filesystem::path& slot_directory) {
    return slot_directory / "history.log";
}

std::filesystem::path TurnCommit::marker_path(
    const std::filesystem::path& slot_directory) {
    return slot_directory / "history.commit";
}

void TurnCommit::load_marker() {
    if (slot_directory_.empty()) {
        committed_seq_ = 0;
        return;
    }
    std::error_code error;
    const auto path = marker_path(slot_directory_);
    if (!std::filesystem::exists(path, error)) {
        if (error) {
            throw std::runtime_error{"無法檢查 history.commit：" + error.message()};
        }
        committed_seq_ = 0;
        marker_exists_ = false;
        return;
    }
    marker_exists_ = true;
    committed_seq_ = decode_marker(path);
    if (committed_seq_ > history_.head_seq()) {
        throw std::runtime_error{"history.commit 超過日誌鏈頭：marker=" +
                                 std::to_string(committed_seq_) + " head=" +
                                 std::to_string(history_.head_seq())};
    }
}

void TurnCommit::ensure_marker() {
    if (slot_directory_.empty()) {
        throw std::logic_error{"turn commit 尚未綁定世界槽"};
    }
    const auto path = marker_path(slot_directory_);
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        if (error) {
            throw std::runtime_error{"無法檢查 history.commit：" + error.message()};
        }
        write_marker(committed_seq_);
    }
}

void TurnCommit::write_marker(std::uint64_t value) {
    atomic_write(marker_path(slot_directory_), marker_bytes(value));
    committed_seq_ = value;
    marker_exists_ = true;
}

} // namespace aetheria::runtime

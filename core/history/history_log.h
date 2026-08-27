#pragma once

// history_log.h：payload 無關的 append-only 歷史日誌與雜湊鏈。

#include "core/time/tick.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace aetheria::history {

struct HistoryEntry {
    std::uint64_t seq{};
    time::Tick tick{};
    std::string kind;
    std::string payload;
    std::uint64_t prev_hash{};
    std::uint64_t entry_hash{};

    bool operator==(const HistoryEntry&) const = default;
};

class HistoryLog {
public:
    explicit HistoryLog(std::uint64_t genesis_hash);
    HistoryLog(std::filesystem::path path, std::uint64_t genesis_hash);

    const HistoryEntry& append(time::Tick tick, std::string_view kind,
                               std::string_view payload);
    [[nodiscard]] const std::vector<HistoryEntry>& entries() const noexcept {
        return entries_;
    }
    [[nodiscard]] std::uint64_t head_hash() const noexcept;
    [[nodiscard]] std::uint64_t head_seq() const noexcept;
    [[nodiscard]] std::uint64_t genesis_hash() const noexcept {
        return genesis_hash_;
    }
    [[nodiscard]] bool bound() const noexcept { return !path_.empty(); }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

    void bind(std::filesystem::path path);
    void verify() const;
    void replay_after(
        std::uint64_t committed_seq,
        const std::function<void(const HistoryEntry&)>& apply) const;

private:
    void load();
    void create_file();
    void append_bytes(const HistoryEntry& entry) const;

    std::filesystem::path path_;
    std::uint64_t genesis_hash_{};
    std::vector<HistoryEntry> entries_;
};

}  // namespace aetheria::history

#pragma once

// turn_commit.h：世界級 journal-first commit point 與玩家命令重放協調器。

#include "core/history/history_log.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string_view>

namespace aetheria::zone {
class ZoneManager;
class ZoneStore;
}

namespace aetheria::runtime {

class PlayableSession;

class TurnCommit {
public:
    TurnCommit() = default;
    explicit TurnCommit(std::filesystem::path slot_directory);

    [[nodiscard]] const history::HistoryEntry&
    record(time::Tick tick, std::string_view kind, std::string_view payload);
    void attach(const std::filesystem::path& slot_directory);

    [[nodiscard]] std::uint64_t committed_seq() const noexcept {
        return committed_seq_;
    }
    [[nodiscard]] bool recovery_needed() const noexcept {
        return !marker_exists_ || history_.head_seq() > committed_seq_;
    }
    [[nodiscard]] const history::HistoryLog& history() const noexcept {
        return history_;
    }

    void replay_tail(PlayableSession& session) const;
    void replay_all(PlayableSession& session) const;

    // zone/manifest 與同一交易的附屬狀態全部成功後才前推 marker。
    void commit_world(zone::ZoneStore& active_store,
                      zone::ZoneStore& destination, zone::ZoneManager& manager,
                      std::uint64_t world_seed, std::uint64_t raws_hash,
                      time::Tick now,
                      const std::function<void()>& persist_before_marker = {});

    void set_interrupt_after_journal_for_testing(bool enabled) noexcept {
        interrupt_after_journal_ = enabled;
    }
    void set_interrupt_after_save_for_testing(bool enabled) noexcept {
        interrupt_after_save_ = enabled;
    }

    [[nodiscard]] static std::filesystem::path
    log_path(const std::filesystem::path& slot_directory);
    [[nodiscard]] static std::filesystem::path
    marker_path(const std::filesystem::path& slot_directory);

private:
    void load_marker();
    void ensure_marker();
    void write_marker(std::uint64_t value);
    void apply_entry(PlayableSession& session,
                     const history::HistoryEntry& entry) const;

    history::HistoryLog history_;
    std::filesystem::path slot_directory_;
    std::uint64_t committed_seq_{};
    bool marker_exists_{};
    bool interrupt_after_journal_{};
    bool interrupt_after_save_{};
};

}  // namespace aetheria::runtime

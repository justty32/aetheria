#include "core/history/history_log.h"

#include "core/base/check.h"

#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <type_traits>

#include "core/history/history_log_detail.h"

namespace aetheria::history {
using namespace detail;

namespace {

constexpr std::uint64_t kFnvOffset = UINT64_C(14695981039346656037);
constexpr std::uint64_t kFnvPrime = UINT64_C(1099511628211);

void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= kFnvPrime;
}

template <typename Value>
void hash_scalar(std::uint64_t& hash, Value value) noexcept {
    static_assert(std::is_integral_v<Value>);
    using Unsigned = std::make_unsigned_t<Value>;
    auto bits = static_cast<Unsigned>(value);
    for (std::size_t byte = 0; byte < sizeof(bits); ++byte) {
        hash_byte(hash, static_cast<std::uint8_t>(bits & UINT8_MAX));
        bits >>= 8U;
    }
}

void hash_string(std::uint64_t& hash, std::string_view value) noexcept {
    hash_scalar(hash, static_cast<std::uint64_t>(value.size()));
    for (const auto byte : value) {
        hash_byte(hash, static_cast<std::uint8_t>(static_cast<unsigned char>(byte)));
    }
}

[[nodiscard]] std::uint64_t calculate_hash(const HistoryEntry& entry) noexcept {
    auto hash = kFnvOffset;
    hash_scalar(hash, entry.prev_hash);
    hash_scalar(hash, entry.seq);
    hash_scalar(hash, static_cast<std::int64_t>(entry.tick));
    hash_string(hash, entry.kind);
    hash_string(hash, entry.payload);
    return hash;
}

} // namespace

namespace detail {

[[noreturn]] void broken(std::uint64_t seq, std::string_view reason) {
    throw std::runtime_error{"history 雜湊鏈在第 " + std::to_string(seq) +
                             " 筆斷裂：" + std::string{reason}};
}

} // namespace detail

HistoryLog::HistoryLog(std::filesystem::path path) : path_{std::move(path)} {
    load();
}

const HistoryEntry& HistoryLog::append(time::Tick tick, std::string_view kind,
                                       std::string_view payload) {
    if (kind.empty()) {
        throw std::invalid_argument{"history kind 不得為空"};
    }
    HistoryEntry entry{head_seq() + 1U, tick, std::string{kind}, std::string{payload},
                       head_hash(), 0};
    entry.entry_hash = calculate_hash(entry);
    if (bound()) {
        if (!std::filesystem::exists(path_)) {
            create_file();
        }
        append_bytes(entry);
    }
    entries_.push_back(std::move(entry));
    return entries_.back();
}

std::uint64_t HistoryLog::head_hash() const noexcept {
    return entries_.empty() ? kHistoryGenesisHash : entries_.back().entry_hash;
}

std::uint64_t HistoryLog::head_seq() const noexcept {
    return entries_.empty() ? 0U : entries_.back().seq;
}

void HistoryLog::bind(std::filesystem::path path) {
    if (path.empty()) {
        throw std::invalid_argument{"history.log 路徑不得為空"};
    }
    if (bound() && path_.lexically_normal() != path.lexically_normal()) {
        throw std::logic_error{"history log 已綁定另一個世界槽"};
    }
    if (bound()) {
        return;
    }
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        throw std::runtime_error{"無法檢查 history.log：" + error.message()};
    }
    if (exists) {
        HistoryLog existing{path};
        if (existing.entries_ != entries_) {
            throw std::runtime_error{"目的槽 history.log 與目前 session 歷史不一致"};
        }
        path_ = std::move(path);
        return;
    }
    path_ = std::move(path);
    create_file();
    for (const auto& entry : entries_) {
        append_bytes(entry);
    }
}

void HistoryLog::verify() const {
    auto expected_prev = kHistoryGenesisHash;
    std::uint64_t expected_seq{1};
    for (const auto& entry : entries_) {
        if (entry.seq != expected_seq) {
            broken(expected_seq, "seq 不連續");
        }
        if (entry.prev_hash != expected_prev) {
            broken(entry.seq, "prev_hash 不符");
        }
        if (entry.entry_hash != calculate_hash(entry)) {
            broken(entry.seq, "entry_hash 不符");
        }
        expected_prev = entry.entry_hash;
        ++expected_seq;
    }
}

void HistoryLog::replay_after(
    std::uint64_t committed_seq,
    const std::function<void(const HistoryEntry&)>& apply) const {
    if (!apply) {
        throw std::invalid_argument{"history replay callback 不得為空"};
    }
    if (committed_seq > head_seq()) {
        throw std::runtime_error{"history.commit 超過日誌鏈頭"};
    }
    for (const auto& entry : entries_) {
        if (entry.seq > committed_seq) {
            apply(entry);
        }
    }
}

} // namespace aetheria::history

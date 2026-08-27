#include "core/history/history_log.h"

#include "core/base/check.h"

#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <type_traits>

namespace aetheria::history {
namespace {

constexpr std::string_view kMagic{"AETHERIA_HISTORY_1\n"};
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

template <typename Value>
void write_scalar(std::ostream& stream, Value value) {
    static_assert(std::is_integral_v<Value>);
    using Unsigned = std::make_unsigned_t<Value>;
    auto bits = static_cast<Unsigned>(value);
    std::array<char, sizeof(Value)> bytes{};
    for (auto& byte : bytes) {
        byte = static_cast<char>(bits & UINT8_MAX);
        bits >>= 8U;
    }
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

template <typename Value>
[[nodiscard]] bool read_scalar(std::istream& stream, Value& value) {
    static_assert(std::is_integral_v<Value>);
    using Unsigned = std::make_unsigned_t<Value>;
    std::array<unsigned char, sizeof(Value)> bytes{};
    stream.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        return false;
    }
    Unsigned bits{};
    for (std::size_t index = bytes.size(); index-- > 0;) {
        bits <<= 8U;
        bits |= bytes[index];
    }
    value = static_cast<Value>(bits);
    return true;
}

[[noreturn]] void broken(std::uint64_t seq, std::string_view reason) {
    throw std::runtime_error{"history 雜湊鏈在第 " + std::to_string(seq) +
                             " 筆斷裂：" + std::string{reason}};
}

}  // namespace

HistoryLog::HistoryLog(std::uint64_t genesis_hash) : genesis_hash_{genesis_hash} {}

HistoryLog::HistoryLog(std::filesystem::path path, std::uint64_t genesis_hash)
    : path_{std::move(path)}, genesis_hash_{genesis_hash} {
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
    return entries_.empty() ? genesis_hash_ : entries_.back().entry_hash;
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
        HistoryLog existing{path, genesis_hash_};
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
    auto expected_prev = genesis_hash_;
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

void HistoryLog::load() {
    std::error_code error;
    if (!std::filesystem::exists(path_, error)) {
        if (error) {
            throw std::runtime_error{"無法檢查 history.log：" + error.message()};
        }
        return;
    }
    std::ifstream stream{path_, std::ios::binary};
    if (!stream) {
        throw std::runtime_error{"無法開啟 history.log：" + path_.string()};
    }
    std::string magic(kMagic.size(), '\0');
    stream.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (!stream || magic != kMagic) {
        broken(1, "檔頭不符");
    }
    std::uint64_t expected_seq{1};
    while (stream.peek() != std::char_traits<char>::eof()) {
        HistoryEntry entry;
        std::int64_t tick{};
        std::uint32_t kind_size{};
        std::uint64_t payload_size{};
        if (!read_scalar(stream, entry.seq) || !read_scalar(stream, tick) ||
            !read_scalar(stream, kind_size) || !read_scalar(stream, payload_size) ||
            !read_scalar(stream, entry.prev_hash) || !read_scalar(stream, entry.entry_hash)) {
            broken(expected_seq, "固定欄位遭截斷");
        }
        if (kind_size == 0U || kind_size > 4096U ||
            payload_size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
            broken(expected_seq, "字串長度無效");
        }
        entry.tick = time::Tick{tick};
        entry.kind.resize(kind_size);
        entry.payload.resize(static_cast<std::size_t>(payload_size));
        stream.read(entry.kind.data(), static_cast<std::streamsize>(entry.kind.size()));
        if (!entry.payload.empty()) {
            stream.read(entry.payload.data(),
                        static_cast<std::streamsize>(entry.payload.size()));
        }
        if (!stream) {
            broken(expected_seq, "kind 或 payload 遭截斷");
        }
        entries_.push_back(std::move(entry));
        ++expected_seq;
    }
    verify();
}

void HistoryLog::create_file() {
    std::filesystem::create_directories(path_.parent_path());
    std::ofstream stream{path_, std::ios::binary | std::ios::trunc};
    stream.write(kMagic.data(), static_cast<std::streamsize>(kMagic.size()));
    stream.flush();
    if (!stream) {
        throw std::runtime_error{"無法建立 history.log：" + path_.string()};
    }
}

void HistoryLog::append_bytes(const HistoryEntry& entry) const {
    AETH_CHECK(bound());
    std::ofstream stream{path_, std::ios::binary | std::ios::app};
    if (!stream) {
        throw std::runtime_error{"無法追加 history.log：" + path_.string()};
    }
    write_scalar(stream, entry.seq);
    write_scalar(stream, static_cast<std::int64_t>(entry.tick));
    write_scalar(stream, static_cast<std::uint32_t>(entry.kind.size()));
    write_scalar(stream, static_cast<std::uint64_t>(entry.payload.size()));
    write_scalar(stream, entry.prev_hash);
    write_scalar(stream, entry.entry_hash);
    stream.write(entry.kind.data(), static_cast<std::streamsize>(entry.kind.size()));
    stream.write(entry.payload.data(), static_cast<std::streamsize>(entry.payload.size()));
    stream.flush();
    if (!stream) {
        throw std::runtime_error{"history.log 追加失敗：" + path_.string()};
    }
}

}  // namespace aetheria::history

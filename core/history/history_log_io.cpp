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

constexpr std::string_view kMagic{"AETHERIA_HISTORY_1\n"};
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

} // namespace

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

} // namespace aetheria::history

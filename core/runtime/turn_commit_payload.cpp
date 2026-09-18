#include "core/runtime/turn_commit_detail.h"

#include <charconv>
#include <stdexcept>
#include <system_error>

namespace aetheria::runtime::detail {

[[nodiscard]] std::uint64_t parse_u64(std::string_view payload,
                                      std::string_view key) {
    const auto prefix = std::string{key} + " = ";
    const auto position = payload.find(prefix);
    if (position == std::string_view::npos) {
        throw std::runtime_error{"history payload 缺欄位：" + std::string{key}};
    }
    const auto begin = payload.data() + position + prefix.size();
    const auto line_end = payload.find('\n', position + prefix.size());
    const auto end = line_end == std::string_view::npos
                         ? payload.data() + payload.size()
                         : payload.data() + line_end;
    std::uint64_t value{};
    const auto [parsed_end, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || parsed_end != end) {
        throw std::runtime_error{"history payload 整數無效：" + std::string{key}};
    }
    return value;
}

[[nodiscard]] std::int64_t parse_i64(std::string_view payload,
                                     std::string_view key) {
    const auto prefix = std::string{key} + " = ";
    const auto position = payload.find(prefix);
    if (position == std::string_view::npos) {
        throw std::runtime_error{"history payload 缺欄位：" + std::string{key}};
    }
    const auto begin = payload.data() + position + prefix.size();
    const auto line_end = payload.find('\n', position + prefix.size());
    const auto end = line_end == std::string_view::npos
                         ? payload.data() + payload.size()
                         : payload.data() + line_end;
    std::int64_t value{};
    const auto [parsed_end, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || parsed_end != end) {
        throw std::runtime_error{"history payload 整數無效：" + std::string{key}};
    }
    return value;
}

[[nodiscard]] std::string parse_string(std::string_view payload,
                                       std::string_view key) {
    const auto prefix = std::string{key} + " = \"";
    const auto position = payload.find(prefix);
    if (position == std::string_view::npos) {
        throw std::runtime_error{"history payload 缺欄位：" + std::string{key}};
    }
    const auto value_begin = position + prefix.size();
    const auto value_end = payload.find('"', value_begin);
    if (value_end == std::string_view::npos) {
        throw std::runtime_error{"history payload 字串無效：" + std::string{key}};
    }
    return std::string{payload.substr(value_begin, value_end - value_begin)};
}

} // namespace aetheria::runtime::detail

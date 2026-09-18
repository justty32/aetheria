#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace aetheria::runtime::detail {

[[nodiscard]] std::uint64_t parse_u64(std::string_view payload,
                                      std::string_view key);
[[nodiscard]] std::int64_t parse_i64(std::string_view payload,
                                     std::string_view key);
[[nodiscard]] std::string parse_string(std::string_view payload,
                                       std::string_view key);

} // namespace aetheria::runtime::detail

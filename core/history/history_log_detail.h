#pragma once

#include <cstdint>
#include <string_view>

namespace aetheria::history::detail {

[[noreturn]] void broken(std::uint64_t seq, std::string_view reason);

} // namespace aetheria::history::detail

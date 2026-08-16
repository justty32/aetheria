#pragma once

#include <cstdio>
#include <cstdlib>

namespace aetheria::detail {

[[noreturn]] inline void check_failed(const char* condition, const char* file, int line) noexcept {
    std::fprintf(stderr, "AETH_CHECK failed: %s (%s:%d)\n", condition, file, line);
    std::fflush(stderr);
    std::abort();
}

}  // namespace aetheria::detail

#define AETH_CHECK(condition)                                                                      \
    do {                                                                                           \
        if (!(condition)) [[unlikely]] {                                                           \
            ::aetheria::detail::check_failed(#condition, __FILE__, __LINE__);                      \
        }                                                                                          \
    } while (false)

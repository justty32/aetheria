#pragma once

// 效能測試固定暖機一次，再量固定 N 次取最小值；不得重試到通過。

#include <algorithm>
#include <cstddef>
#include <limits>

namespace aetheria::tests {

inline constexpr std::size_t kPerformanceSampleCount = 5;

template <typename Measure>
[[nodiscard]] double minimum_milliseconds_after_warmup(Measure&& measure) {
    static_cast<void>(measure());
    double minimum = std::numeric_limits<double>::max();
    for (std::size_t sample = 0; sample < kPerformanceSampleCount; ++sample) {
        minimum = std::min(minimum, measure());
    }
    return minimum;
}

}  // namespace aetheria::tests

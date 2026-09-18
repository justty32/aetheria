#pragma once

// 目標、行動與成長共用的飽和整數評分。

#include <algorithm>
#include <cstdint>
#include <limits>

namespace aetheria::ai::detail {

[[nodiscard]] inline std::int32_t saturating_score(std::int64_t value) noexcept {
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(
        value, std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::max()));
}

} // namespace aetheria::ai::detail

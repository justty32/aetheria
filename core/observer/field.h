#pragma once

// Observer 場強的共用純函式；空間 LOD 與勢力 AI LOD 都以 strength - travel_cost 分級。

#include <algorithm>
#include <cstdint>
#include <limits>

namespace aetheria::observer {

[[nodiscard]] constexpr std::int32_t field_score(std::int32_t strength,
                                                 std::int32_t travel_cost) noexcept {
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(
        static_cast<std::int64_t>(strength) - travel_cost,
        std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::max()));
}

[[nodiscard]] constexpr std::int32_t strength_for_score(
    std::int32_t desired_score, std::int32_t travel_cost) noexcept {
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(
        static_cast<std::int64_t>(desired_score) + travel_cost,
        std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::max()));
}

} // namespace aetheria::observer

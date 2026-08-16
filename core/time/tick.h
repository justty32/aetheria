#pragma once

#include <cstdint>

namespace aetheria::time {

// Tick 是全局時間軸上的秒數。
// 它是無擁有者的值型別。
// 值本身永不失效。
enum class Tick : std::int64_t {};

inline constexpr Tick kSecondsPerXun{864'000};

// CalendarDate 是 360 天曆中的旬精度日期。
// 它是無擁有者的值型別。
// 值本身永不失效。
struct CalendarDate {
    std::int32_t year;
    std::uint8_t season;
    std::uint8_t month;
    std::uint8_t xun;

    constexpr bool operator==(const CalendarDate&) const noexcept = default;
};

// Tick 0 是第 1 年、第 1 季、第 1 月、第 1 旬的起點；欄位皆從 1 起算。
[[nodiscard]] CalendarDate to_date(Tick tick) noexcept;

// 回傳指定旬第一秒的 Tick；輸入欄位須是有效的 1-based 日期。
[[nodiscard]] Tick to_tick(CalendarDate date) noexcept;

}  // namespace aetheria::time

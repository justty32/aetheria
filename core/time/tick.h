#pragma once

#include <compare>
#include <cstdint>
#include <limits>

namespace aetheria::time {

// Tick 是全局時間軸上的秒數。
// 它是無擁有者的值型別。
// 值本身永不失效。
enum class Tick : std::int64_t {};

// Duration 是兩個時刻之間的秒數。
// 它是無擁有者的值型別。
// 值本身永不失效。
enum class Duration : std::int64_t {};

[[nodiscard]] constexpr Tick operator+(Tick tick, Duration duration) noexcept {
    return Tick{static_cast<std::int64_t>(tick) + static_cast<std::int64_t>(duration)};
}

[[nodiscard]] constexpr Tick operator+(Duration duration, Tick tick) noexcept {
    return tick + duration;
}

[[nodiscard]] constexpr Tick operator-(Tick tick, Duration duration) noexcept {
    return Tick{static_cast<std::int64_t>(tick) - static_cast<std::int64_t>(duration)};
}

[[nodiscard]] constexpr Duration operator-(Tick lhs, Tick rhs) noexcept {
    return Duration{static_cast<std::int64_t>(lhs) - static_cast<std::int64_t>(rhs)};
}

[[nodiscard]] constexpr Duration operator+(Duration lhs, Duration rhs) noexcept {
    return Duration{static_cast<std::int64_t>(lhs) + static_cast<std::int64_t>(rhs)};
}

[[nodiscard]] constexpr Duration operator-(Duration lhs, Duration rhs) noexcept {
    return Duration{static_cast<std::int64_t>(lhs) - static_cast<std::int64_t>(rhs)};
}

[[nodiscard]] constexpr Duration operator*(Duration duration, std::int64_t multiplier) noexcept {
    return Duration{static_cast<std::int64_t>(duration) * multiplier};
}

[[nodiscard]] constexpr Duration operator*(std::int64_t multiplier, Duration duration) noexcept {
    return duration * multiplier;
}

[[nodiscard]] constexpr std::int64_t operator/(Duration lhs, Duration rhs) noexcept {
    return static_cast<std::int64_t>(lhs) / static_cast<std::int64_t>(rhs);
}

[[nodiscard]] constexpr Duration operator%(Duration lhs, Duration rhs) noexcept {
    return Duration{static_cast<std::int64_t>(lhs) % static_cast<std::int64_t>(rhs)};
}

inline constexpr Duration kXun{864'000};
inline constexpr Duration kYear{31'104'000};
inline constexpr Duration kHour{3'600};
inline constexpr Duration kMinute{60};
inline constexpr Duration kSiteCombatTurn{900};
inline constexpr Duration kLocalCombatTurn{6};

inline constexpr Tick kMinTick{
    (static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()) - 1) *
    static_cast<std::int64_t>(kYear)};
inline constexpr Tick kMaxTick{static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) *
                                   static_cast<std::int64_t>(kYear) -
                               1};

[[nodiscard]] constexpr bool is_representable(Tick tick) noexcept {
    return tick >= kMinTick && tick <= kMaxTick;
}

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

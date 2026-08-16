#include "core/time/tick.h"

#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>

#include <gtest/gtest.h>

namespace {

using aetheria::time::CalendarDate;
using aetheria::time::Duration;
using aetheria::time::is_representable;
using aetheria::time::kHour;
using aetheria::time::kLocalCombatTurn;
using aetheria::time::kMaxTick;
using aetheria::time::kMinTick;
using aetheria::time::kMinute;
using aetheria::time::kSiteCombatTurn;
using aetheria::time::kXun;
using aetheria::time::kYear;
using aetheria::time::Tick;
using aetheria::time::to_date;
using aetheria::time::to_tick;

template <typename Lhs, typename Rhs>
concept Addable = requires(Lhs lhs, Rhs rhs) { lhs + rhs; };

template <typename Lhs, typename Rhs>
concept Subtractable = requires(Lhs lhs, Rhs rhs) { lhs - rhs; };

template <typename Lhs, typename Rhs>
concept Multipliable = requires(Lhs lhs, Rhs rhs) { lhs * rhs; };

template <typename Lhs, typename Rhs>
concept Dividable = requires(Lhs lhs, Rhs rhs) { lhs / rhs; };

static_assert(Addable<Tick, Duration>);
static_assert(Addable<Duration, Tick>);
static_assert(Subtractable<Tick, Duration>);
static_assert(std::same_as<decltype(Tick{} - Tick{}), Duration>);
static_assert(std::same_as<decltype(Duration{} + Duration{}), Duration>);
static_assert(std::same_as<decltype(Duration{} / Duration{1}), std::int64_t>);
static_assert(std::same_as<decltype(Tick{} <=> Tick{}), std::strong_ordering>);
static_assert(std::same_as<decltype(Duration{} <=> Duration{}), std::strong_ordering>);
static_assert(!Addable<Tick, Tick>);
static_assert(!Multipliable<Tick, std::int64_t>);
static_assert(!Multipliable<std::int64_t, Tick>);
static_assert(!Dividable<Tick, std::int64_t>);

static_assert(kXun == Duration{864'000});
static_assert(kYear == Duration{31'104'000});
static_assert(kHour == Duration{3'600});
static_assert(kMinute == Duration{60});
static_assert(kSiteCombatTurn == Duration{900});
static_assert(kLocalCombatTurn == Duration{6});

TEST(TimeArithmetic, PreservesPointAndIntervalTypes) {
    constexpr Tick start{1'000};
    constexpr Duration stride{60};

    static_assert(start + stride == Tick{1'060});
    static_assert(stride + start == Tick{1'060});
    static_assert(start - stride == Tick{940});
    static_assert((start + stride) - start == stride);
    static_assert((stride * 3) / stride == 3);
    static_assert((Duration{125} % stride) == Duration{5});
}

TEST(CalendarConversion, ConvertsEpochAndCalendarBoundaries) {
    EXPECT_EQ(to_date(Tick{0}), (CalendarDate{1, 1, 1, 1}));
    EXPECT_EQ(to_date(Tick{0} + kXun), (CalendarDate{1, 1, 1, 2}));
    EXPECT_EQ(to_date(Tick{0} + kXun * 2), (CalendarDate{1, 1, 1, 3}));
    EXPECT_EQ(to_date(Tick{0} + kXun * 3), (CalendarDate{1, 1, 2, 1}));
    EXPECT_EQ(to_date(Tick{0} + kXun * 9), (CalendarDate{1, 2, 1, 1}));
    EXPECT_EQ(to_date(Tick{0} + kXun * 36), (CalendarDate{2, 1, 1, 1}));
}

TEST(CalendarConversion, HandlesEntireRepresentableBoundary) {
    const CalendarDate minimum_date{std::numeric_limits<std::int32_t>::min(), 1, 1, 1};
    const CalendarDate maximum_date{std::numeric_limits<std::int32_t>::max(), 4, 3, 3};

    EXPECT_TRUE(is_representable(kMinTick));
    EXPECT_TRUE(is_representable(kMaxTick));
    EXPECT_EQ(to_date(kMinTick), minimum_date);
    EXPECT_EQ(to_tick(minimum_date), kMinTick);
    EXPECT_EQ(to_date(kMaxTick), maximum_date);
    EXPECT_EQ(to_date(to_tick(maximum_date)), maximum_date);
}

TEST(CalendarConversion, RejectsTicksOutsideRepresentableBoundary) {
    EXPECT_DEATH(static_cast<void>(to_date(kMinTick - Duration{1})),
                 "AETH_CHECK failed: is_representable\\(tick\\)");
    EXPECT_DEATH(static_cast<void>(to_date(kMaxTick + Duration{1})),
                 "AETH_CHECK failed: is_representable\\(tick\\)");
}

TEST(CalendarConversion, RejectsInvalidOneBasedDateFields) {
    EXPECT_DEATH(static_cast<void>(to_tick(CalendarDate{1, 0, 1, 1})),
                 "AETH_CHECK failed: date.season");
    EXPECT_DEATH(static_cast<void>(to_tick(CalendarDate{1, 1, 4, 1})),
                 "AETH_CHECK failed: date.month");
    EXPECT_DEATH(static_cast<void>(to_tick(CalendarDate{1, 1, 1, 4})),
                 "AETH_CHECK failed: date.xun");
}

TEST(CalendarConversion, RoundTripsEveryXunInRepresentativeYears) {
    for (std::int32_t year = -2; year <= 20; ++year) {
        for (std::uint8_t season = 1; season <= 4; ++season) {
            for (std::uint8_t month = 1; month <= 3; ++month) {
                for (std::uint8_t xun = 1; xun <= 3; ++xun) {
                    const CalendarDate date{year, season, month, xun};
                    EXPECT_EQ(to_date(to_tick(date)), date);
                }
            }
        }
    }
}

}  // namespace

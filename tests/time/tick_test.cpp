#include "core/time/tick.h"

#include <cstdint>

#include <gtest/gtest.h>

namespace {

using aetheria::time::CalendarDate;
using aetheria::time::Tick;
using aetheria::time::kSecondsPerXun;
using aetheria::time::to_date;
using aetheria::time::to_tick;

TEST(CalendarConversion, ConvertsEpochAndCalendarBoundaries) {
    EXPECT_EQ(to_date(Tick{0}), (CalendarDate{1, 1, 1, 1}));
    EXPECT_EQ(to_date(kSecondsPerXun), (CalendarDate{1, 1, 1, 2}));
    EXPECT_EQ(to_date(Tick{2 * 864'000}), (CalendarDate{1, 1, 1, 3}));
    EXPECT_EQ(to_date(Tick{3 * 864'000}), (CalendarDate{1, 1, 2, 1}));
    EXPECT_EQ(to_date(Tick{9 * 864'000}), (CalendarDate{1, 2, 1, 1}));
    EXPECT_EQ(to_date(Tick{36 * 864'000}), (CalendarDate{2, 1, 1, 1}));
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


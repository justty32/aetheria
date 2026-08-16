#include "core/time/tick.h"

#include <cassert>

namespace aetheria::time {
namespace {

constexpr std::int64_t kXunPerMonth = 3;
constexpr std::int64_t kMonthsPerSeason = 3;
constexpr std::int64_t kSeasonsPerYear = 4;
constexpr std::int64_t kXunPerSeason = kXunPerMonth * kMonthsPerSeason;
constexpr std::int64_t kXunPerYear = kXunPerSeason * kSeasonsPerYear;

constexpr std::int64_t floor_div(std::int64_t numerator, std::int64_t denominator) noexcept {
    const auto quotient = numerator / denominator;
    const auto remainder = numerator % denominator;
    return quotient - (remainder < 0 ? 1 : 0);
}

}  // namespace

CalendarDate to_date(Tick tick) noexcept {
    const auto seconds = static_cast<std::int64_t>(tick);
    const auto elapsed_xun = floor_div(seconds, static_cast<std::int64_t>(kSecondsPerXun));
    const auto elapsed_years = floor_div(elapsed_xun, kXunPerYear);
    const auto xun_in_year = elapsed_xun - elapsed_years * kXunPerYear;

    const auto season_index = xun_in_year / kXunPerSeason;
    const auto xun_in_season = xun_in_year % kXunPerSeason;
    const auto month_index = xun_in_season / kXunPerMonth;
    const auto xun_index = xun_in_season % kXunPerMonth;

    return CalendarDate{
        .year = static_cast<std::int32_t>(elapsed_years + 1),
        .season = static_cast<std::uint8_t>(season_index + 1),
        .month = static_cast<std::uint8_t>(month_index + 1),
        .xun = static_cast<std::uint8_t>(xun_index + 1),
    };
}

Tick to_tick(CalendarDate date) noexcept {
    assert(date.season >= 1 && date.season <= kSeasonsPerYear);
    assert(date.month >= 1 && date.month <= kMonthsPerSeason);
    assert(date.xun >= 1 && date.xun <= kXunPerMonth);

    const auto elapsed_years = static_cast<std::int64_t>(date.year) - 1;
    const auto season_index = static_cast<std::int64_t>(date.season) - 1;
    const auto month_index = static_cast<std::int64_t>(date.month) - 1;
    const auto xun_index = static_cast<std::int64_t>(date.xun) - 1;
    const auto elapsed_xun = elapsed_years * kXunPerYear + season_index * kXunPerSeason +
                             month_index * kXunPerMonth + xun_index;
    return Tick{elapsed_xun * static_cast<std::int64_t>(kSecondsPerXun)};
}

}  // namespace aetheria::time

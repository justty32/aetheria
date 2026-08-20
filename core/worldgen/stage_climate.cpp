#include "core/worldgen/region_climate_stages.h"

#include "core/worldgen/gen_grid.h"
#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <vector>

namespace aetheria::worldgen {
namespace {

constexpr auto kFullWindPercent = 100;
constexpr auto kWindTransitionHalfWidthDegrees = 5;

[[nodiscard]] constexpr std::int8_t zonal_wind_percent(int absolute_latitude) noexcept {
    constexpr auto first_boundary = 30;
    constexpr auto second_boundary = 60;
    if (absolute_latitude < first_boundary - kWindTransitionHalfWidthDegrees) {
        return -kFullWindPercent;
    }
    if (absolute_latitude <= first_boundary + kWindTransitionHalfWidthDegrees) {
        return static_cast<std::int8_t>(
            -kFullWindPercent +
            (absolute_latitude - (first_boundary - kWindTransitionHalfWidthDegrees)) *
                kFullWindPercent / kWindTransitionHalfWidthDegrees);
    }
    if (absolute_latitude < second_boundary - kWindTransitionHalfWidthDegrees) {
        return kFullWindPercent;
    }
    if (absolute_latitude <= second_boundary + kWindTransitionHalfWidthDegrees) {
        return static_cast<std::int8_t>(
            kFullWindPercent -
            (absolute_latitude - (second_boundary - kWindTransitionHalfWidthDegrees)) *
                kFullWindPercent / kWindTransitionHalfWidthDegrees);
    }
    return -kFullWindPercent;
}

}  // namespace

ClimateStageOutput generate_climate(const RegionSlowVariables& slow,
                                    const QuantizedElevation& elevation, std::uint64_t stage_seed,
                                    const ClimateGenerationConfig& config) {
    const auto count = detail::checked_count(elevation.width, elevation.height);
    if (slow.width != elevation.width || slow.height != elevation.height ||
        elevation.meters.size() != count || elevation.land.size() != count ||
        slow.latitude_degrees < -90 || slow.latitude_degrees > 90 ||
        config.air_retention_percent > 100U) {
        throw std::invalid_argument{"氣候階段輸入尺寸、緯度或水氣保留率無效"};
    }
    constexpr std::array<std::int16_t, 7> latitude_temperature{300, 280, 230, 160, 70, -50, -180};
    ClimateStageOutput output{elevation.width, elevation.height, {}, {}, {}};
    output.temperature_tenths.resize(count);
    output.moisture.resize(count);
    output.prevailing_wind_x.resize(elevation.height);

    for (std::uint32_t y = 0; y < elevation.height; ++y) {
        const auto row_offset =
            elevation.height == 1 ? 0 : static_cast<int>(y * 20U / (elevation.height - 1U)) - 10;
        const auto latitude =
            std::clamp(static_cast<int>(slow.latitude_degrees) + row_offset, -90, 90);
        const auto absolute_latitude = std::abs(latitude);
        const auto band = static_cast<std::size_t>(absolute_latitude / 15);
        const auto remainder = absolute_latitude % 15;
        const auto next_band = std::min(band + 1U, latitude_temperature.size() - 1U);
        const auto base_temperature =
            static_cast<std::int32_t>((latitude_temperature[band] * (15 - remainder) +
                                       latitude_temperature[next_band] * remainder) /
                                      15);
        const auto wind_percent = zonal_wind_percent(absolute_latitude);
        output.prevailing_wind_x[y] = wind_percent;
        for (std::uint32_t x = 0; x < elevation.width; ++x) {
            const auto index = static_cast<std::size_t>(y) * elevation.width + x;
            const auto encoded_elevation = static_cast<std::int32_t>(elevation.meters[index]);
            const auto meters_above_zero = std::max(0, encoded_elevation - 4096);
            const auto lapse = meters_above_zero * config.lapse_tenths_per_km / 1000;
            output.temperature_tenths[index] = static_cast<std::int16_t>(
                std::clamp(base_temperature - static_cast<std::int32_t>(lapse),
                           static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::min()),
                           static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::max())));
        }
        const auto moisture_pass = [&](int direction) {
            std::vector<std::uint16_t> row(elevation.width);
            auto air =
                static_cast<std::uint32_t>(30000U + splitmix64(stage_seed ^ y) % UINT64_C(20001));
            auto previous_elevation = static_cast<std::int32_t>(elevation.sea_level);
            for (std::uint32_t step = 0; step < elevation.width; ++step) {
                const auto x = direction > 0 ? step : elevation.width - 1U - step;
                const auto index = static_cast<std::size_t>(y) * elevation.width + x;
                const auto encoded_elevation = static_cast<std::int32_t>(elevation.meters[index]);
                if (elevation.land[index] == 0) {
                    air = UINT16_MAX;
                    row[x] = UINT16_MAX;
                } else {
                    const auto rise = std::max(0, encoded_elevation - previous_elevation);
                    const auto uplift_force = static_cast<std::uint32_t>(rise) * config.uplift_rain;
                    const auto uplift_rain = static_cast<std::uint32_t>(
                        static_cast<std::uint64_t>(air) * uplift_force /
                        (static_cast<std::uint64_t>(UINT16_MAX) + uplift_force));
                    row[x] = static_cast<std::uint16_t>(
                        std::min<std::uint32_t>(UINT16_MAX, air + uplift_rain));
                    air -= uplift_rain;
                    air = (air * config.air_retention_percent + kFullWindPercent - 1U) /
                          kFullWindPercent;
                }
                previous_elevation = encoded_elevation;
            }
            return row;
        };
        const auto eastward_weight =
            static_cast<std::uint32_t>(wind_percent + kFullWindPercent) / 2U;
        if (eastward_weight == 0U || eastward_weight == kFullWindPercent) {
            const auto row = moisture_pass(eastward_weight == 0U ? -1 : 1);
            std::copy(row.begin(), row.end(),
                      output.moisture.begin() + static_cast<std::size_t>(y) * elevation.width);
            continue;
        }
        const auto eastward = moisture_pass(1);
        const auto westward = moisture_pass(-1);
        for (std::uint32_t x = 0; x < elevation.width; ++x) {
            const auto index = static_cast<std::size_t>(y) * elevation.width + x;
            const auto blended =
                static_cast<std::uint32_t>(eastward[x]) * eastward_weight +
                static_cast<std::uint32_t>(westward[x]) * (kFullWindPercent - eastward_weight);
            output.moisture[index] =
                static_cast<std::uint16_t>((blended + kFullWindPercent / 2U) / kFullWindPercent);
        }
    }
    return output;
}

}  // namespace aetheria::worldgen

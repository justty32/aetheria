#include "core/worldgen/region_climate_stages.h"

#include "core/worldgen/gen_grid.h"
#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace aetheria::worldgen {

RiverStageOutput generate_rivers(const QuantizedElevation& elevation,
                                 const ClimateStageOutput& climate, std::uint64_t stage_seed,
                                 const RiverGenerationConfig& config) {
    const auto count = detail::checked_count(elevation.width, elevation.height);
    if (elevation.meters.size() != count || elevation.land.size() != count ||
        climate.width != elevation.width || climate.height != elevation.height ||
        climate.moisture.size() != count || config.stream_threshold == 0 ||
        config.stream_threshold > config.river_threshold ||
        config.river_threshold > config.great_river_threshold) {
        throw std::invalid_argument{"河流階段輸入尺寸或門檻無效"};
    }

    RiverStageOutput output{
        elevation.width, elevation.height, elevation.meters, {}, {}, {}, climate.moisture, {}};
    output.downstream.assign(count, -1);
    output.flow.resize(count);
    output.river_class.assign(count, 0);
    output.lake.assign(count, 0);
    std::vector<std::int32_t> bucket_head(UINT16_MAX + 1U, -1);
    std::vector<std::int32_t> bucket_next(count, -1);
    std::vector<std::uint8_t> visited(count);
    std::vector<std::size_t> order;
    order.reserve(count);

    auto push = [&](std::size_t index, std::uint16_t level) {
        bucket_next[index] = bucket_head[level];
        bucket_head[level] = static_cast<std::int32_t>(index);
        visited[index] = 1;
    };
    for (std::uint32_t y = 0; y < elevation.height; ++y) {
        for (std::uint32_t x = 0; x < elevation.width; ++x) {
            const auto index = static_cast<std::size_t>(y) * elevation.width + x;
            const bool boundary =
                x == 0 || y == 0 || x + 1U == elevation.width || y + 1U == elevation.height;
            if (elevation.land[index] == 0 || boundary) {
                push(index, output.filled_elevation[index]);
                output.lake[index] = elevation.land[index] != 0 ? UINT8_C(1) : UINT8_C(0);
            }
        }
    }

    std::size_t processed{};
    std::uint32_t level{};
    while (processed < count) {
        while (level <= UINT16_MAX && bucket_head[level] < 0) {
            ++level;
        }
        if (level > UINT16_MAX) {
            throw std::runtime_error{"priority-flood 未能覆蓋全圖"};
        }
        const auto index = static_cast<std::size_t>(bucket_head[level]);
        bucket_head[level] = bucket_next[index];
        order.push_back(index);
        ++processed;
        for (const auto neighbor : detail::neighbors(index, elevation.width, elevation.height)) {
            if (neighbor >= count || visited[neighbor] != 0) {
                continue;
            }
            output.filled_elevation[neighbor] = std::max<std::uint16_t>(
                output.filled_elevation[neighbor], static_cast<std::uint16_t>(level));
            output.downstream[neighbor] = static_cast<std::int32_t>(index);
            push(neighbor, output.filled_elevation[neighbor]);
        }
    }

    // Priority-flood 的 parent 已替平地提供無環出口；若存在更低的填平後鄰格，
    // 則明確指向其中最低者。嚴格下降邊不可能成環，等高邊仍沿 flood 樹。
    for (std::size_t index = 0; index < count; ++index) {
        if (elevation.land[index] == 0 || output.downstream[index] < 0) {
            continue;
        }
        const auto adjacent = detail::neighbors(index, elevation.width, elevation.height);
        const auto rotation = static_cast<std::size_t>(
            splitmix64(stage_seed ^ static_cast<std::uint64_t>(index)) % adjacent.size());
        auto lowest = static_cast<std::size_t>(output.downstream[index]);
        for (std::size_t offset = 0; offset < adjacent.size(); ++offset) {
            const auto neighbor = adjacent[(rotation + offset) % adjacent.size()];
            if (neighbor < count &&
                output.filled_elevation[neighbor] < output.filled_elevation[lowest]) {
                lowest = neighbor;
            }
        }
        if (output.filled_elevation[lowest] < output.filled_elevation[index]) {
            output.downstream[index] = static_cast<std::int32_t>(lowest);
        }
    }

    for (std::size_t index = 0; index < count; ++index) {
        output.flow[index] = 1U + climate.moisture[index];
    }
    for (auto iterator = order.rbegin(); iterator != order.rend(); ++iterator) {
        const auto index = *iterator;
        const auto downstream = output.downstream[index];
        if (downstream >= 0) {
            auto& target = output.flow[static_cast<std::size_t>(downstream)];
            target =
                target > UINT32_MAX - output.flow[index] ? UINT32_MAX : target + output.flow[index];
        }
    }
    for (std::size_t index = 0; index < count; ++index) {
        if (elevation.land[index] == 0 || output.downstream[index] < 0) {
            continue;
        }
        output.river_class[index] = output.flow[index] >= config.great_river_threshold ? UINT8_C(3)
                                    : output.flow[index] >= config.river_threshold     ? UINT8_C(2)
                                    : output.flow[index] >= config.stream_threshold    ? UINT8_C(1)
                                                                                       : UINT8_C(0);
        if (output.river_class[index] != 0) {
            for (const auto neighbor : detail::neighbors(index, elevation.width, elevation.height)) {
                if (neighbor < count) {
                    output.moisture[neighbor] = static_cast<std::uint16_t>(std::min<std::uint32_t>(
                        UINT16_MAX, output.moisture[neighbor] + config.moisture_bonus));
                }
            }
        }
    }
    return output;
}

}  // namespace aetheria::worldgen

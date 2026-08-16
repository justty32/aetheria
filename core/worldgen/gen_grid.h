#pragma once

// gen_grid.h 收斂 Region 生成器內部共用的網格尺寸檢查、四鄰格計算與陸地連通分量。

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace aetheria::worldgen::detail {

[[nodiscard]] inline std::size_t checked_count(std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0 || width > UINT16_MAX || height > UINT16_MAX) {
        throw std::invalid_argument{"Region 生成尺寸必須落在 1..65535"};
    }
    const auto count64 = static_cast<std::uint64_t>(width) * height;
    if (count64 > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument{"Region 生成尺寸超出可表達範圍"};
    }
    return static_cast<std::size_t>(count64);
}

[[nodiscard]] inline std::array<std::size_t, 4> neighbors(std::size_t index, std::uint32_t width,
                                                          std::uint32_t height) noexcept {
    const auto x = index % width;
    const auto y = index / width;
    constexpr auto missing = std::numeric_limits<std::size_t>::max();
    return {y > 0 ? index - width : missing, x + 1 < width ? index + 1 : missing,
            y + 1 < height ? index + width : missing, x > 0 ? index - 1 : missing};
}

[[nodiscard]] inline std::vector<std::size_t>
largest_land_component(const std::vector<std::uint8_t>& land, std::uint32_t width,
                       std::uint32_t height) {
    std::vector<std::uint8_t> visited(land.size());
    std::vector<std::size_t> largest;
    std::queue<std::size_t> pending;
    for (std::size_t start = 0; start < land.size(); ++start) {
        if (land[start] == 0 || visited[start] != 0) {
            continue;
        }
        std::vector<std::size_t> component;
        visited[start] = 1;
        pending.push(start);
        while (!pending.empty()) {
            const auto current = pending.front();
            pending.pop();
            component.push_back(current);
            for (const auto neighbor : neighbors(current, width, height)) {
                if (neighbor < land.size() && land[neighbor] != 0 && visited[neighbor] == 0) {
                    visited[neighbor] = 1;
                    pending.push(neighbor);
                }
            }
        }
        if (component.size() > largest.size()) {
            largest = std::move(component);
        }
    }
    return largest;
}

}  // namespace aetheria::worldgen::detail

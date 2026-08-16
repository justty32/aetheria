#include "core/worldgen/city_scoring.h"

#include "core/worldgen/civ_tiles.h"

#include <array>
#include <queue>

namespace aetheria::worldgen::detail {

std::vector<std::uint8_t>
ocean_connected_to_boundary(const QuantizedElevation& elevation) {
    const auto count = elevation.land.size();
    std::vector<std::uint8_t> ocean(count);
    std::queue<std::size_t> open;
    for (std::size_t index = 0; index < count; ++index) {
        const auto x = index % elevation.width;
        const auto y = index / elevation.width;
        if (elevation.land[index] == 0 &&
            (x == 0 || y == 0 || x + 1 == elevation.width || y + 1 == elevation.height)) {
            ocean[index] = 1;
            open.push(index);
        }
    }
    while (!open.empty()) {
        const auto current = open.front();
        open.pop();
        for (const auto next : neighbors(current, elevation.width, elevation.height)) {
            if (next < count && elevation.land[next] == 0 && ocean[next] == 0) {
                ocean[next] = 1;
                open.push(next);
            }
        }
    }
    return ocean;
}

std::uint16_t local_bottleneck_score(const QuantizedElevation& elevation,
                                     std::size_t removed, std::uint8_t radius) {
    const auto starts = neighbors(removed, elevation.width, elevation.height);
    std::array<std::uint8_t, 289> visited{};
    std::array<std::size_t, 289> open{};
    const auto center_x = static_cast<int>(removed % elevation.width);
    const auto center_y = static_cast<int>(removed / elevation.width);
    const auto side = static_cast<std::size_t>(radius) * 2U + 1U;
    auto local_index = [&](std::size_t global) {
        const auto x = static_cast<int>(global % elevation.width) - center_x + radius;
        const auto y = static_cast<int>(global / elevation.width) - center_y + radius;
        return static_cast<std::size_t>(y) * side + static_cast<std::size_t>(x);
    };
    auto inside = [&](std::size_t global) {
        const auto x = static_cast<int>(global % elevation.width);
        const auto y = static_cast<int>(global / elevation.width);
        return std::abs(x - center_x) <= radius && std::abs(y - center_y) <= radius;
    };
    std::uint16_t components{};
    for (const auto start : starts) {
        if (start >= elevation.land.size() || elevation.land[start] == 0 ||
            visited[local_index(start)] != 0) {
            continue;
        }
        ++components;
        std::size_t head{};
        std::size_t tail{};
        open[tail++] = start;
        visited[local_index(start)] = 1;
        while (head < tail) {
            const auto current = open[head++];
            for (const auto next : neighbors(current, elevation.width, elevation.height)) {
                if (next >= elevation.land.size() || next == removed || elevation.land[next] == 0 ||
                    !inside(next)) {
                    continue;
                }
                const auto local = local_index(next);
                if (visited[local] == 0) {
                    visited[local] = 1;
                    open[tail++] = next;
                }
            }
        }
    }
    return components > 0 ? static_cast<std::uint16_t>(components - 1U) : 0;
}

}  // namespace aetheria::worldgen::detail

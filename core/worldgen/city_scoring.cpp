#include "core/worldgen/city_scoring.h"

#include "core/worldgen/civ_tiles.h"

#include <array>
#include <limits>
#include <queue>
#include <stdexcept>

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

std::vector<std::uint8_t>
bottleneck_passability_mask(const QuantizedElevation& elevation, const BiomeStageOutput& biome,
                            const FeatureStageOutput& features, const rules::Ruleset& ruleset,
                            std::uint16_t barrier_move_cost) {
    std::vector<std::uint8_t> passable(elevation.land.size());
    for (std::size_t index = 0; index < passable.size(); ++index) {
        const auto* terrain = ruleset.terrain(biome.terrain[index]);
        const auto* relief = ruleset.relief(biome.relief[index]);
        const auto* feature = ruleset.feature(features.feature[index]);
        if (terrain == nullptr || relief == nullptr || feature == nullptr) {
            throw std::runtime_error{"瓶頸遮罩含不存在的 terrain／relief／feature"};
        }
        const auto cost = static_cast<std::int64_t>(terrain->move_cost) + relief->move_cost +
                          feature->move_cost;
        const bool water = (terrain->flags & rules::kTerrainWaterFlag) != 0;
        passable[index] = static_cast<std::uint8_t>(
            elevation.land[index] != 0 && !water && cost < barrier_move_cost);
    }
    return passable;
}

std::uint16_t local_bottleneck_score(std::span<const std::uint8_t> passable,
                                     std::uint32_t width, std::uint32_t height,
                                     std::size_t removed, std::uint8_t radius) {
    if (passable.size() != static_cast<std::size_t>(width) * height || removed >= passable.size() ||
        passable[removed] == 0) {
        return 0;
    }
    const auto starts = neighbors(removed, width, height);
    std::array<std::uint8_t, 289> visited{};
    std::array<std::size_t, 289> open{};
    const auto center_x = static_cast<int>(removed % width);
    const auto center_y = static_cast<int>(removed / width);
    const auto side = static_cast<std::size_t>(radius) * 2U + 1U;
    auto local_index = [&](std::size_t global) {
        const auto x = static_cast<int>(global % width) - center_x + radius;
        const auto y = static_cast<int>(global / width) - center_y + radius;
        return static_cast<std::size_t>(y) * side + static_cast<std::size_t>(x);
    };
    auto inside = [&](std::size_t global) {
        const auto x = static_cast<int>(global % width);
        const auto y = static_cast<int>(global / width);
        return std::abs(x - center_x) <= radius && std::abs(y - center_y) <= radius;
    };
    std::uint16_t components{};
    for (const auto start : starts) {
        if (start >= passable.size() || passable[start] == 0 ||
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
            for (const auto next : neighbors(current, width, height)) {
                if (next >= passable.size() || next == removed || passable[next] == 0 ||
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

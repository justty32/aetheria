#include "core/worldgen/portal_candidates.h"

#include <optional>
#include <stdexcept>

namespace aetheria::worldgen::detail {
namespace {

enum class CostPreference : std::uint8_t {
    Lowest,
    Highest,
};

[[nodiscard]] bool is_water(const world::RegionTiles& tiles, std::size_t index,
                            const rules::Ruleset& ruleset) {
    const auto* terrain = ruleset.terrain(tiles.base.at(index));
    if (terrain == nullptr) {
        throw std::runtime_error{"出境點解析遇到無效 TerrainId"};
    }
    return (terrain->flags & rules::kTerrainWaterFlag) != 0;
}

[[nodiscard]] bool is_boundary(std::size_t index, std::uint32_t width,
                               std::uint32_t height) noexcept {
    const auto x = index % width;
    const auto y = index / width;
    return x == 0 || y == 0 || x + 1U == width || y + 1U == height;
}

[[nodiscard]] std::int64_t tile_move_cost(const world::RegionTiles& tiles, std::size_t index,
                                          const rules::Ruleset& ruleset) {
    const auto* terrain = ruleset.terrain(tiles.base.at(index));
    const auto* relief = ruleset.relief(tiles.relief.at(index));
    const auto* feature = ruleset.feature(tiles.feature.at(index));
    if (terrain == nullptr || relief == nullptr || feature == nullptr) {
        throw std::runtime_error{"出境點解析遇到無效 definition id"};
    }
    return static_cast<std::int64_t>(terrain->move_cost) + relief->move_cost + feature->move_cost;
}

template <typename Predicate>
[[nodiscard]] std::optional<std::size_t>
best_index(const world::RegionTiles& tiles, const rules::Ruleset& ruleset,
           std::span<const std::uint8_t> occupied, CostPreference preference,
           Predicate&& predicate) {
    std::optional<std::size_t> best;
    for (std::size_t index = 0; index < tiles.tile_count(); ++index) {
        if (occupied[index] != 0 || !predicate(index)) {
            continue;
        }
        const auto cost = tile_move_cost(tiles, index, ruleset);
        const auto best_cost = best.has_value() ? tile_move_cost(tiles, *best, ruleset) : 0;
        if (!best.has_value() || (preference == CostPreference::Lowest && cost < best_cost) ||
            (preference == CostPreference::Highest && cost > best_cost) ||
            (cost == best_cost && index < *best)) {
            best = index;
        }
    }
    return best;
}

}  // namespace

std::size_t resolve_boundary_portal(const world::RegionTiles& tiles, const rules::Ruleset& ruleset,
                                    bool underground, std::span<const std::uint8_t> occupied) {
    if (occupied.size() != tiles.tile_count()) {
        throw std::invalid_argument{"出境點已佔用格集合尺寸無效"};
    }
    const auto mountain = ruleset.find_relief("relief.mountain");
    const auto boundary_land = [&](std::size_t index) {
        return is_boundary(index, tiles.width, tiles.height) && !is_water(tiles, index, ruleset);
    };
    if (underground) {
        const auto ruin =
            best_index(tiles, ruleset, occupied, CostPreference::Lowest, [&](std::size_t index) {
                if (!boundary_land(index)) {
                    return false;
                }
                const auto* feature = ruleset.feature(tiles.feature[index]);
                return feature != nullptr && (feature->flags & rules::kFeatureRuinFlag) != 0;
            });
        const auto deepest_mountain =
            best_index(tiles, ruleset, occupied, CostPreference::Highest, [&](std::size_t index) {
                return boundary_land(index) && mountain.has_value() &&
                       tiles.relief[index] == *mountain;
            });
        const auto fallback =
            ruin.has_value() ? ruin
            : deepest_mountain.has_value()
                ? deepest_mountain
                : best_index(tiles, ruleset, occupied, CostPreference::Lowest, boundary_land);
        if (!fallback.has_value()) {
            throw std::runtime_error{"地下通道找不到未佔用的可通行邊界格"};
        }
        return *fallback;
    }
    const auto preferred =
        best_index(tiles, ruleset, occupied, CostPreference::Lowest, [&](std::size_t index) {
            return boundary_land(index) && mountain.has_value() && tiles.relief[index] == *mountain;
        });
    const auto fallback = preferred.has_value() ? preferred
                                                : best_index(tiles, ruleset, occupied,
                                                             CostPreference::Lowest, boundary_land);
    if (!fallback.has_value()) {
        throw std::runtime_error{"邊界通道找不到可通行邊界格"};
    }
    return *fallback;
}

}  // namespace aetheria::worldgen::detail

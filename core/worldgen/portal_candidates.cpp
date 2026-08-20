#include "core/worldgen/portal_candidates.h"

#include "core/worldgen/civ_tiles.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace aetheria::worldgen::detail {
namespace {

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

[[nodiscard]] bool is_coastal(const world::RegionTiles& tiles, std::size_t index,
                              const rules::Ruleset& ruleset) {
    if (is_water(tiles, index, ruleset)) {
        return false;
    }
    return std::ranges::any_of(neighbors(index, tiles.width, tiles.height),
                               [&](std::size_t neighbor) {
                                   return neighbor < tiles.tile_count() &&
                                          is_water(tiles, neighbor, ruleset);
                               });
}

[[nodiscard]] std::int64_t tile_move_cost(const world::RegionTiles& tiles, std::size_t index,
                                          const rules::Ruleset& ruleset) {
    const auto* terrain = ruleset.terrain(tiles.base.at(index));
    const auto* relief = ruleset.relief(tiles.relief.at(index));
    const auto* feature = ruleset.feature(tiles.feature.at(index));
    if (terrain == nullptr || relief == nullptr || feature == nullptr) {
        throw std::runtime_error{"出境點解析遇到無效 definition id"};
    }
    return static_cast<std::int64_t>(terrain->move_cost) + relief->move_cost +
           feature->move_cost;
}

template <typename Predicate>
[[nodiscard]] std::optional<std::size_t>
best_index(const world::RegionTiles& tiles, const rules::Ruleset& ruleset, Predicate&& predicate) {
    std::optional<std::size_t> best;
    for (std::size_t index = 0; index < tiles.tile_count(); ++index) {
        if (!predicate(index)) {
            continue;
        }
        if (!best.has_value() ||
            std::pair{tile_move_cost(tiles, index, ruleset), index} <
                std::pair{tile_move_cost(tiles, *best, ruleset), *best}) {
            best = index;
        }
    }
    return best;
}

}  // namespace

std::size_t resolve_sea_portal(world::RegionTiles& tiles, CityStageOutput& cities,
                               const rules::Ruleset& ruleset) {
    std::optional<CitySite> best_city;
    for (const auto& city : cities.cities) {
        if (!is_coastal(tiles, city.canonical_id, ruleset)) {
            continue;
        }
        if (!best_city.has_value() ||
            std::pair{city.score, std::numeric_limits<std::uint32_t>::max() - city.canonical_id} >
                std::pair{best_city->score,
                          std::numeric_limits<std::uint32_t>::max() - best_city->canonical_id}) {
            best_city = city;
        }
    }
    if (best_city.has_value()) {
        return best_city->canonical_id;
    }
    std::optional<std::size_t> best;
    for (std::size_t index = 0; index < tiles.tile_count(); ++index) {
        if (!is_coastal(tiles, index, ruleset)) {
            continue;
        }
        if (!best.has_value() ||
            std::pair{cities.score[index], std::numeric_limits<std::size_t>::max() - index} >
                std::pair{cities.score[*best],
                          std::numeric_limits<std::size_t>::max() - *best}) {
            best = index;
        }
    }
    if (!best.has_value()) {
        throw std::runtime_error{"海路通道找不到沿海陸格"};
    }
    const auto tile = coordinate(*best, tiles.width);
    cities.cities.push_back({static_cast<std::uint32_t>(*best), tile, cities.score[*best],
                             world::SettlementTier::Town, 0});
    tiles.settlement[*best] = world::SettlementTier::Town;
    std::ranges::sort(cities.cities, {}, &CitySite::canonical_id);
    return *best;
}

std::size_t resolve_boundary_portal(const world::RegionTiles& tiles,
                                    const rules::Ruleset& ruleset, bool underground) {
    const auto mountain = ruleset.find_relief("relief.mountain");
    const auto preferred = best_index(tiles, ruleset, [&](std::size_t index) {
        if (!is_boundary(index, tiles.width, tiles.height) ||
            is_water(tiles, index, ruleset)) {
            return false;
        }
        const auto* feature = ruleset.feature(tiles.feature[index]);
        const bool ruin = feature != nullptr && (feature->flags & rules::kFeatureRuinFlag) != 0;
        return (mountain.has_value() && tiles.relief[index] == *mountain) ||
               (underground && ruin);
    });
    const auto fallback = preferred.has_value()
                              ? preferred
                              : best_index(tiles, ruleset, [&](std::size_t index) {
                                    return is_boundary(index, tiles.width, tiles.height) &&
                                           !is_water(tiles, index, ruleset);
                                });
    if (!fallback.has_value()) {
        throw std::runtime_error{"邊界通道找不到可通行邊界格"};
    }
    return *fallback;
}

std::size_t resolve_teleport_portal(const world::RegionTiles& tiles,
                                    const rules::WorldGraphConnection& connection,
                                    bool endpoint_a, const rules::Ruleset& ruleset) {
    const auto& specified = endpoint_a ? connection.coordinate_a : connection.coordinate_b;
    if (specified.has_value()) {
        const world::RegionXY tile{specified->x, specified->y};
        const auto index = tiles.index_of(tile);
        if (is_water(tiles, index, ruleset)) {
            throw std::runtime_error{"傳送門資料指定在不可通行水格"};
        }
        return index;
    }
    const auto landmark = ruleset.find_feature("feature.landmark");
    if (!landmark.has_value()) {
        throw std::runtime_error{"傳送門解析缺少 feature.landmark"};
    }
    const auto center_x = static_cast<std::int32_t>(tiles.width / 2U);
    const auto center_y = static_cast<std::int32_t>(tiles.height / 2U);
    std::optional<std::size_t> best;
    std::int64_t best_distance{std::numeric_limits<std::int64_t>::max()};
    for (std::size_t index = 0; index < tiles.tile_count(); ++index) {
        if (tiles.feature[index] != *landmark || is_water(tiles, index, ruleset)) {
            continue;
        }
        const auto tile = coordinate(index, tiles.width);
        const auto distance = std::abs(static_cast<std::int32_t>(tile.x) - center_x) +
                              std::abs(static_cast<std::int32_t>(tile.y) - center_y);
        if (!best.has_value() || std::pair{distance, index} < std::pair{best_distance, *best}) {
            best = index;
            best_distance = distance;
        }
    }
    if (!best.has_value()) {
        throw std::runtime_error{"傳送門找不到 feature.landmark"};
    }
    return *best;
}

}  // namespace aetheria::worldgen::detail

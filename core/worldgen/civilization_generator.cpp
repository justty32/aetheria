#include "core/worldgen/region_generator.h"

#include "core/world/region_movement.h"

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace aetheria::worldgen {
namespace {

inline constexpr std::size_t kMissing = std::numeric_limits<std::size_t>::max();

[[nodiscard]] std::size_t checked_count(std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0 || width > static_cast<std::uint32_t>(INT16_MAX) ||
        height > static_cast<std::uint32_t>(INT16_MAX)) {
        throw std::invalid_argument{"文明生成尺寸必須落在 1..32767"};
    }
    return static_cast<std::size_t>(width) * height;
}

[[nodiscard]] std::array<std::size_t, 4> neighbors(std::size_t index, std::uint32_t width,
                                                   std::uint32_t height) noexcept {
    const auto x = index % width;
    const auto y = index / width;
    return {y > 0 ? index - width : kMissing, x + 1 < width ? index + 1 : kMissing,
            y + 1 < height ? index + width : kMissing, x > 0 ? index - 1 : kMissing};
}

[[nodiscard]] world::RegionXY coordinate(std::size_t index, std::uint32_t width) noexcept {
    return {static_cast<std::int16_t>(index % width), static_cast<std::int16_t>(index / width)};
}

[[nodiscard]] std::uint32_t manhattan(world::RegionXY lhs, world::RegionXY rhs) noexcept {
    return static_cast<std::uint32_t>(std::abs(static_cast<int>(lhs.x) - static_cast<int>(rhs.x)) +
                                      std::abs(static_cast<int>(lhs.y) - static_cast<int>(rhs.y)));
}

void require_civilization_inputs(const QuantizedElevation& elevation,
                                 const ClimateStageOutput& climate, const RiverStageOutput& rivers,
                                 const BiomeStageOutput& biome,
                                 const FeatureStageOutput& features) {
    const auto count = checked_count(elevation.width, elevation.height);
    if (climate.width != elevation.width || climate.height != elevation.height ||
        rivers.width != elevation.width || rivers.height != elevation.height ||
        biome.width != elevation.width || biome.height != elevation.height ||
        features.width != elevation.width || features.height != elevation.height ||
        elevation.meters.size() != count || elevation.land.size() != count ||
        climate.temperature_tenths.size() != count || rivers.downstream.size() != count ||
        rivers.river_class.size() != count || rivers.moisture.size() != count ||
        rivers.lake.size() != count || biome.terrain.size() != count ||
        biome.relief.size() != count || features.feature.size() != count) {
        throw std::invalid_argument{"文明生成階段輸入尺寸不一致"};
    }
}

[[nodiscard]] std::vector<std::uint8_t>
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

[[nodiscard]] std::uint16_t local_bottleneck_score(const QuantizedElevation& elevation,
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

void install_rivers(world::RegionTiles& tiles, const RiverStageOutput& rivers,
                    const RegionDefinitionIds& definitions) {
    for (std::size_t index = 0; index < rivers.river_class.size(); ++index) {
        const auto target_raw = rivers.downstream[index];
        const auto river_class = rivers.river_class[index];
        if (river_class == 0 || target_raw < 0) {
            continue;
        }
        const auto target = static_cast<std::size_t>(target_raw);
        if (target >= rivers.river_class.size()) {
            throw std::runtime_error{"河流 downstream 超出文明階段輸入"};
        }
        const auto edge = river_class == 3   ? definitions.great_river
                          : river_class == 2 ? definitions.river
                                             : definitions.stream;
        tiles.set_edge(coordinate(index, tiles.width), coordinate(target, tiles.width), edge);
    }
}

[[nodiscard]] world::RegionTiles
make_base_tiles(const QuantizedElevation& elevation, const ClimateStageOutput& climate,
                const RiverStageOutput& rivers, const BiomeStageOutput& biome,
                const FeatureStageOutput& features, const RegionDefinitionIds& definitions) {
    world::RegionTiles tiles{elevation.width, elevation.height};
    tiles.base = biome.terrain;
    tiles.relief = biome.relief;
    tiles.feature = features.feature;
    tiles.elevation = elevation.meters;
    for (std::size_t index = 0; index < elevation.meters.size(); ++index) {
        tiles.temperature[index] = static_cast<std::uint8_t>(std::clamp<std::int32_t>(
            (static_cast<std::int32_t>(climate.temperature_tenths[index]) + 500) * 255 / 1000, 0,
            UINT8_MAX));
        tiles.moisture[index] = static_cast<std::uint8_t>(rivers.moisture[index] / 257U);
    }
    std::fill(tiles.edges.begin(), tiles.edges.end(), definitions.no_edge);
    install_rivers(tiles, rivers, definitions);
    return tiles;
}

struct DisjointSet {
    explicit DisjointSet(std::size_t count) : parent(count), rank(count) {
        std::iota(parent.begin(), parent.end(), 0U);
    }

    [[nodiscard]] std::size_t find(std::size_t value) {
        if (parent[value] != value) {
            parent[value] = find(parent[value]);
        }
        return parent[value];
    }

    [[nodiscard]] bool unite(std::size_t lhs, std::size_t rhs) {
        lhs = find(lhs);
        rhs = find(rhs);
        if (lhs == rhs) {
            return false;
        }
        if (rank[lhs] < rank[rhs]) {
            std::swap(lhs, rhs);
        }
        parent[rhs] = lhs;
        if (rank[lhs] == rank[rhs]) {
            ++rank[lhs];
        }
        return true;
    }

    std::vector<std::size_t> parent;
    std::vector<std::uint8_t> rank;
};

struct CandidateConnection {
    std::size_t first{};
    std::size_t second{};
    std::int64_t cost{};
    bool selected{};
};

[[nodiscard]] std::int64_t tree_distance(std::size_t start, std::size_t goal,
                                         const std::vector<CandidateConnection>& tree,
                                         std::size_t city_count) {
    std::vector<std::vector<std::pair<std::size_t, std::int64_t>>> graph(city_count);
    for (const auto& edge : tree) {
        graph[edge.first].push_back({edge.second, edge.cost});
        graph[edge.second].push_back({edge.first, edge.cost});
    }
    std::vector<std::int64_t> distance(city_count, -1);
    std::queue<std::size_t> open;
    distance[start] = 0;
    open.push(start);
    while (!open.empty()) {
        const auto current = open.front();
        open.pop();
        if (current == goal) {
            return distance[current];
        }
        for (const auto [next, cost] : graph[current]) {
            if (distance[next] < 0) {
                distance[next] = distance[current] + cost;
                open.push(next);
            }
        }
    }
    throw std::runtime_error{"MST tree distance 不連通"};
}

[[nodiscard]] std::optional<rules::EdgeId>
compound_edge(const rules::CivilizationRules& civilization, rules::EdgeId river,
              rules::EdgeId road) {
    for (const auto& crossing : civilization.crossings) {
        if (crossing.river == river && crossing.road == road) {
            return crossing.result;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<rules::EdgeId>
underlying_river(const rules::CivilizationRules& civilization, rules::EdgeId edge,
                 const rules::Ruleset& ruleset) {
    const auto* definition = ruleset.edge(edge);
    if (definition == nullptr || (definition->flags & rules::kEdgeRiverFlag) == 0) {
        return std::nullopt;
    }
    for (const auto& crossing : civilization.crossings) {
        if (crossing.result == edge) {
            return crossing.river;
        }
    }
    return edge;
}

[[nodiscard]] std::size_t directed_offset(std::size_t from, std::size_t to, std::uint32_t width) {
    if (to + width == from) {
        return from * 4U;
    }
    if (to == from + 1U) {
        return from * 4U + 1U;
    }
    if (to == from + width) {
        return from * 4U + 2U;
    }
    if (to + 1U == from) {
        return from * 4U + 3U;
    }
    throw std::runtime_error{"道路 path 含非四鄰接 step"};
}

[[nodiscard]] std::int64_t engineering_step_cost(const world::RegionTiles& tiles,
                                                 const RiverStageOutput& rivers, std::size_t from,
                                                 std::size_t to, const rules::Ruleset& ruleset,
                                                 const rules::CivilizationRules& civilization) {
    const auto* terrain = ruleset.terrain(tiles.base[to]);
    const auto* relief = ruleset.relief(tiles.relief[to]);
    if (terrain == nullptr || relief == nullptr ||
        (terrain->flags & rules::kTerrainWaterFlag) != 0) {
        return std::numeric_limits<std::int64_t>::max();
    }
    auto cost = static_cast<std::int64_t>(civilization.road_base_cost) +
                static_cast<std::int64_t>(terrain->move_cost + relief->move_cost) *
                    civilization.road_terrain_weight;
    const auto slope =
        std::abs(static_cast<int>(tiles.elevation[from]) - static_cast<int>(tiles.elevation[to]));
    cost += static_cast<std::int64_t>(slope / civilization.road_slope_divisor) *
            civilization.road_slope_weight;
    if (tiles.base[to] == civilization.swamp_terrain) {
        cost += civilization.road_swamp_penalty;
    }
    if (rivers.river_class[from] != 0 || rivers.river_class[to] != 0) {
        cost -= std::min<std::int64_t>(cost - 1, civilization.road_valley_discount);
    }
    const auto edge =
        tiles.edge_between(coordinate(from, tiles.width), coordinate(to, tiles.width));
    const auto* edge_definition = ruleset.edge(edge);
    if (edge_definition == nullptr) {
        throw std::runtime_error{"道路工程遇到無效 EdgeId"};
    }
    if ((edge_definition->flags & rules::kEdgeRiverFlag) != 0) {
        cost += civilization.road_river_crossing_penalty;
    }
    if ((edge_definition->flags & rules::kEdgeRoadFlag) != 0) {
        cost = std::max<std::int64_t>(1, cost * civilization.road_reuse_numerator /
                                             civilization.road_reuse_denominator);
    }
    return cost;
}

[[nodiscard]] std::vector<std::size_t>
find_engineering_path(const world::RegionTiles& tiles, const RiverStageOutput& rivers,
                      std::size_t start, std::size_t goal, const rules::Ruleset& ruleset,
                      const rules::CivilizationRules& civilization) {
    const auto count = tiles.tile_count();
    constexpr auto infinity = std::numeric_limits<std::int64_t>::max();
    std::vector<std::int64_t> distance(count, infinity);
    std::vector<std::size_t> parent(count, kMissing);
    using Candidate = std::pair<std::int64_t, std::size_t>;
    std::priority_queue<Candidate, std::vector<Candidate>, std::greater<>> open;
    distance[start] = 0;
    open.push({0, start});
    while (!open.empty()) {
        const auto [known, current] = open.top();
        open.pop();
        if (known != distance[current]) {
            continue;
        }
        if (current == goal) {
            break;
        }
        for (const auto next : neighbors(current, tiles.width, tiles.height)) {
            if (next >= count) {
                continue;
            }
            const auto step =
                engineering_step_cost(tiles, rivers, current, next, ruleset, civilization);
            if (step == infinity || known > infinity - step || known + step >= distance[next]) {
                continue;
            }
            distance[next] = known + step;
            parent[next] = current;
            open.push({distance[next], next});
        }
    }
    if (distance[goal] == infinity) {
        throw std::runtime_error{"城市道路沒有可行工程路徑"};
    }
    std::vector<std::size_t> result;
    for (auto current = goal;; current = parent[current]) {
        result.push_back(current);
        if (current == start) {
            break;
        }
        if (parent[current] == kMissing) {
            throw std::runtime_error{"道路 parent chain 中斷"};
        }
    }
    std::ranges::reverse(result);
    return result;
}

}  // namespace

CityStageOutput generate_cities(const QuantizedElevation& elevation,
                                const ClimateStageOutput& climate, const RiverStageOutput& rivers,
                                const BiomeStageOutput& biome, const FeatureStageOutput& features,
                                const rules::Ruleset& ruleset, std::uint64_t stage_seed,
                                const CityGenerationConfig& config) {
    require_civilization_inputs(elevation, climate, rivers, biome, features);
    const auto& civilization = ruleset.civilization_rules();
    if (!civilization.loaded) {
        throw std::invalid_argument{"Ruleset 缺少 civilization.toml"};
    }
    const auto count = elevation.meters.size();
    CityStageOutput output{elevation.width, elevation.height, {}, {}, {}};
    output.score.assign(count, std::numeric_limits<std::int32_t>::min());
    output.bottleneck.resize(count);
    const auto open_ocean = ocean_connected_to_boundary(elevation);
    const auto no_feature = ruleset.find_feature("feature.none");
    if (!no_feature.has_value()) {
        throw std::runtime_error{"城市階段缺少 feature.none"};
    }
    for (std::size_t index = 0; index < count; ++index) {
        if (elevation.land[index] == 0) {
            continue;
        }
        output.bottleneck[index] =
            local_bottleneck_score(elevation, index, civilization.bottleneck_radius);
        std::int64_t score{};
        bool freshwater = rivers.river_class[index] != 0;
        bool harbor{};
        std::uint16_t defenses{};
        for (const auto next : neighbors(index, elevation.width, elevation.height)) {
            if (next >= count) {
                continue;
            }
            freshwater = freshwater || rivers.river_class[next] != 0 || rivers.lake[next] != 0;
            harbor = harbor || open_ocean[next] != 0;
            const auto* relief = ruleset.relief(biome.relief[next]);
            if (relief != nullptr && relief->move_cost >= 2) {
                ++defenses;
            }
            if (rivers.river_class[next] != 0) {
                ++defenses;
            }
        }
        if (freshwater) {
            score += civilization.freshwater_weight;
        }
        if (harbor) {
            score += civilization.harbor_weight;
        }
        score += static_cast<std::int64_t>(defenses) * civilization.defense_weight;
        const auto center = coordinate(index, elevation.width);
        std::uint16_t farmland{};
        std::uint16_t resources{};
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                const auto x = static_cast<int>(center.x) + dx;
                const auto y = static_cast<int>(center.y) + dy;
                if (x < 0 || y < 0 || x >= static_cast<int>(elevation.width) ||
                    y >= static_cast<int>(elevation.height)) {
                    continue;
                }
                const auto nearby =
                    static_cast<std::size_t>(y) * elevation.width + static_cast<std::size_t>(x);
                const auto* terrain = ruleset.terrain(biome.terrain[nearby]);
                const auto* relief = ruleset.relief(biome.relief[nearby]);
                if (elevation.land[nearby] != 0 && terrain != nullptr && relief != nullptr &&
                    terrain->yield.food >= 2 && relief->move_cost <= 2) {
                    ++farmland;
                }
                if (features.feature[nearby] != *no_feature) {
                    ++resources;
                }
            }
        }
        score += static_cast<std::int64_t>(farmland) * civilization.farmland_weight;
        score += static_cast<std::int64_t>(resources) * civilization.resource_weight;
        score +=
            static_cast<std::int64_t>(output.bottleneck[index]) * civilization.bottleneck_weight;
        if (climate.temperature_tenths[index] <= 20 || climate.temperature_tenths[index] >= 350 ||
            rivers.moisture[index] <= 8000 || biome.terrain[index] == civilization.swamp_terrain) {
            score += civilization.extreme_climate_penalty;
        }
        if (elevation.meters[index] >= civilization.high_elevation_threshold) {
            score += civilization.high_elevation_penalty;
        }
        output.score[index] = static_cast<std::int32_t>(
            std::clamp<std::int64_t>(score, std::numeric_limits<std::int32_t>::min(),
                                     std::numeric_limits<std::int32_t>::max()));
    }

    std::vector<std::size_t> candidates;
    candidates.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        if (elevation.land[index] != 0 && output.score[index] >= config.minimum_score_bias) {
            candidates.push_back(index);
        }
    }
    std::ranges::sort(candidates, [&](std::size_t lhs, std::size_t rhs) {
        if (output.score[lhs] != output.score[rhs]) {
            return output.score[lhs] > output.score[rhs];
        }
        const auto lhs_priority = splitmix64(stage_seed ^ lhs);
        const auto rhs_priority = splitmix64(stage_seed ^ rhs);
        return lhs_priority != rhs_priority ? lhs_priority > rhs_priority : lhs < rhs;
    });
    for (const auto index : candidates) {
        if (output.cities.size() >= civilization.target_city_count) {
            break;
        }
        const auto accepted = output.cities.size();
        const auto tier = accepted < civilization.major_city_count ? world::SettlementTier::City
                          : accepted < civilization.major_city_count + civilization.town_count
                              ? world::SettlementTier::Town
                              : world::SettlementTier::Village;
        const auto spacing_index = static_cast<std::size_t>(tier) - 1U;
        const auto spacing = civilization.minimum_spacing[spacing_index];
        const auto tile = coordinate(index, elevation.width);
        const bool too_close = std::ranges::any_of(output.cities, [&](const CitySite& city) {
            return manhattan(tile, city.tile) < std::max(spacing, city.minimum_spacing);
        });
        if (!too_close) {
            output.cities.push_back(
                {static_cast<std::uint32_t>(index), tile, output.score[index], tier, spacing});
        }
    }
    if (output.cities.size() < 2) {
        throw std::runtime_error{"城市選址不足兩座，無法建立道路"};
    }
    return output;
}

RoadStageOutput generate_roads(const QuantizedElevation& elevation,
                               const ClimateStageOutput& climate, const RiverStageOutput& rivers,
                               const BiomeStageOutput& biome, const FeatureStageOutput& features,
                               const CityStageOutput& cities,
                               const RegionDefinitionIds& definitions,
                               const rules::Ruleset& ruleset, std::uint64_t stage_seed,
                               const RoadGenerationConfig& config, bool canonicalize_city_order) {
    static_cast<void>(stage_seed);
    require_civilization_inputs(elevation, climate, rivers, biome, features);
    const auto& civilization = ruleset.civilization_rules();
    if (!civilization.loaded || cities.width != elevation.width ||
        cities.height != elevation.height || cities.cities.size() < 2) {
        throw std::invalid_argument{"道路階段缺少有效文明規則或城市"};
    }
    auto ordered_cities = cities.cities;
    std::ranges::sort(ordered_cities, {}, &CitySite::canonical_id);
    if (std::ranges::adjacent_find(ordered_cities, {}, &CitySite::canonical_id) !=
        ordered_cities.end()) {
        throw std::invalid_argument{"道路階段遇到重複 canonical city id"};
    }
    for (const auto& city : ordered_cities) {
        if (city.tile.x < 0 || city.tile.y < 0 ||
            static_cast<std::uint32_t>(city.tile.x) >= elevation.width ||
            static_cast<std::uint32_t>(city.tile.y) >= elevation.height ||
            city.canonical_id >= elevation.meters.size() ||
            city.canonical_id !=
                static_cast<std::uint32_t>(static_cast<std::size_t>(city.tile.y) * elevation.width +
                                           static_cast<std::size_t>(city.tile.x))) {
            throw std::invalid_argument{"道路階段 canonical city id 與座標不符"};
        }
    }
    auto tiles = make_base_tiles(elevation, climate, rivers, biome, features, definitions);
    std::vector<CandidateConnection> candidates;
    for (std::size_t first = 0; first < ordered_cities.size(); ++first) {
        for (std::size_t second = first + 1; second < ordered_cities.size(); ++second) {
            const auto path = world::find_region_path(tiles, ordered_cities[first].tile,
                                                      ordered_cities[second].tile, ruleset, 1);
            if (path.has_value()) {
                candidates.push_back({first, second, path->cost, false});
            }
        }
    }
    std::ranges::sort(candidates, [&](const auto& lhs, const auto& rhs) {
        return std::tuple{lhs.cost, ordered_cities[lhs.first].canonical_id,
                          ordered_cities[lhs.second].canonical_id} <
               std::tuple{rhs.cost, ordered_cities[rhs.first].canonical_id,
                          ordered_cities[rhs.second].canonical_id};
    });
    DisjointSet sets{ordered_cities.size()};
    std::vector<CandidateConnection> tree;
    for (auto& candidate : candidates) {
        if (sets.unite(candidate.first, candidate.second)) {
            candidate.selected = true;
            tree.push_back(candidate);
        }
    }
    if (tree.size() + 1U != ordered_cities.size()) {
        throw std::runtime_error{"城市完全圖無法連通所有城市"};
    }
    struct LoopCandidate {
        std::size_t index{};
        std::int64_t tree_cost{};
    };
    std::vector<LoopCandidate> loops;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        if (!candidates[index].selected) {
            loops.push_back({index, tree_distance(candidates[index].first, candidates[index].second,
                                                  tree, ordered_cities.size())});
        }
    }
    std::ranges::sort(loops, [&](const LoopCandidate& lhs, const LoopCandidate& rhs) {
        const auto& lhs_edge = candidates[lhs.index];
        const auto& rhs_edge = candidates[rhs.index];
        const auto lhs_ratio = lhs.tree_cost * rhs_edge.cost;
        const auto rhs_ratio = rhs.tree_cost * lhs_edge.cost;
        if (lhs_ratio != rhs_ratio) {
            return lhs_ratio > rhs_ratio;
        }
        return std::tuple{lhs_edge.cost, ordered_cities[lhs_edge.first].canonical_id,
                          ordered_cities[lhs_edge.second].canonical_id} <
               std::tuple{rhs_edge.cost, ordered_cities[rhs_edge.first].canonical_id,
                          ordered_cities[rhs_edge.second].canonical_id};
    });
    const auto loop_percent = config.loop_percent_override == 0 ? civilization.loop_percent
                                                                : config.loop_percent_override;
    if (loop_percent < 10 || loop_percent > 20) {
        throw std::invalid_argument{"道路環路比例必須在 10..20"};
    }
    const auto loop_count = std::min<std::size_t>(
        loops.size(), std::max<std::size_t>(1, (tree.size() * loop_percent + 99U) / 100U));
    std::vector<CandidateConnection> selected = tree;
    for (std::size_t index = 0; index < loop_count; ++index) {
        auto edge = candidates[loops[index].index];
        edge.selected = true;
        selected.push_back(edge);
    }

    std::map<std::uint32_t, std::size_t> input_rank;
    for (std::size_t index = 0; index < cities.cities.size(); ++index) {
        input_rank.emplace(cities.cities[index].canonical_id, index);
    }
    std::ranges::sort(selected, [&](const auto& lhs, const auto& rhs) {
        const auto lhs_first = ordered_cities[lhs.first].canonical_id;
        const auto lhs_second = ordered_cities[lhs.second].canonical_id;
        const auto rhs_first = ordered_cities[rhs.first].canonical_id;
        const auto rhs_second = ordered_cities[rhs.second].canonical_id;
        if (canonicalize_city_order) {
            return std::pair{lhs_first, lhs_second} < std::pair{rhs_first, rhs_second};
        }
        return std::pair{std::min(input_rank.at(lhs_first), input_rank.at(lhs_second)),
                         std::max(input_rank.at(lhs_first), input_rank.at(lhs_second))} <
               std::pair{std::min(input_rank.at(rhs_first), input_rank.at(rhs_second)),
                         std::max(input_rank.at(rhs_first), input_rank.at(rhs_second))};
    });

    RoadStageOutput output{elevation.width, elevation.height, {}, {}, {}};
    output.usage.assign(tiles.edges.size(), 0);
    for (const auto& connection : selected) {
        const auto start = ordered_cities[connection.first].canonical_id;
        const auto goal = ordered_cities[connection.second].canonical_id;
        const auto path = find_engineering_path(tiles, rivers, start, goal, ruleset, civilization);
        for (std::size_t step = 1; step < path.size(); ++step) {
            const auto from = path[step - 1U];
            const auto to = path[step];
            const auto forward = directed_offset(from, to, tiles.width);
            const auto backward = directed_offset(to, from, tiles.width);
            const auto usage = static_cast<std::uint16_t>(
                std::min<unsigned>(UINT16_MAX, static_cast<unsigned>(output.usage[forward]) + 1U));
            output.usage[forward] = usage;
            output.usage[backward] = usage;
            std::size_t road_tier{};
            if (usage >= civilization.road_usage_thresholds[2]) {
                road_tier = 2;
            } else if (usage >= civilization.road_usage_thresholds[1]) {
                road_tier = 1;
            }
            const auto road = civilization.road_edges[road_tier];
            const auto existing =
                tiles.edge_between(coordinate(from, tiles.width), coordinate(to, tiles.width));
            const auto river = underlying_river(civilization, existing, ruleset);
            const auto edge = river.has_value() ? compound_edge(civilization, *river, road) : road;
            if (!edge.has_value()) {
                throw std::runtime_error{"civilization.toml 缺少河級 × 道路級複合 def"};
            }
            tiles.set_edge(coordinate(from, tiles.width), coordinate(to, tiles.width), *edge);
        }
        output.connections.push_back(
            {ordered_cities[connection.first].canonical_id,
             ordered_cities[connection.second].canonical_id, connection.cost,
             std::ranges::none_of(tree, [&](const CandidateConnection& edge) {
                 return edge.first == connection.first && edge.second == connection.second;
             })});
    }
    output.edges = std::move(tiles.edges);
    return output;
}

}  // namespace aetheria::worldgen

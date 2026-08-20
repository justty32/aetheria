#include "core/worldgen/civ_tiles.h"
#include "core/worldgen/region_generator.h"
#include "core/worldgen/road_loops.h"
#include "core/worldgen/road_path.h"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <utility>

namespace aetheria::worldgen {

using detail::coordinate;
using detail::CandidateConnection;

RoadStageOutput generate_roads(const QuantizedElevation& elevation,
                               const ClimateStageOutput& climate, const RiverStageOutput& rivers,
                               const BiomeStageOutput& biome, const HistoryStageOutput& history,
                               const CityStageOutput& cities,
                               const RegionDefinitionIds& definitions,
                               const rules::Ruleset& ruleset, std::uint64_t stage_seed,
                               const RoadGenerationConfig& config, bool canonicalize_city_order) {
    static_cast<void>(stage_seed);
    detail::require_civilization_inputs(elevation, climate, rivers, biome, history.features);
    const auto& civilization = ruleset.civilization_rules();
    if (!civilization.loaded || cities.width != elevation.width ||
        cities.height != elevation.height || cities.cities.size() < 2 ||
        history.edges.size() != elevation.meters.size() * 4U) {
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
    auto tiles =
        detail::make_base_tiles(elevation, climate, rivers, biome, history.features, definitions);
    tiles.edges = history.edges;
    auto [candidates, tree] = detail::build_minimum_spanning_tree(tiles, ordered_cities, ruleset);
    auto selected = tree;
    for (auto& edge : detail::select_loop_connections(candidates, tree, ordered_cities,
                                                       civilization, config)) {
        selected.push_back(std::move(edge));
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
        const auto path =
            detail::find_engineering_path(tiles, rivers, start, goal, ruleset, civilization);
        for (std::size_t step = 1; step < path.size(); ++step) {
            const auto from = path[step - 1U];
            const auto to = path[step];
            const auto forward = detail::directed_offset(from, to, tiles.width);
            const auto backward = detail::directed_offset(to, from, tiles.width);
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
            const auto river = detail::underlying_river(civilization, existing, ruleset);
            const auto edge =
                river.has_value() ? detail::compound_edge(civilization, *river, road) : road;
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

#include "core/worldgen/history_roads.h"

#include "core/worldgen/civ_tiles.h"
#include "core/worldgen/road_loops.h"
#include "core/worldgen/road_path.h"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <utility>

namespace aetheria::worldgen::detail {

AncientRoadOutput
build_ancient_roads(const QuantizedElevation& elevation, const ClimateStageOutput& climate,
                    const RiverStageOutput& rivers, const BiomeStageOutput& biome,
                    const FeatureStageOutput& features, const CityStageOutput& ancient_sites,
                    const RegionDefinitionIds& definitions, const rules::Ruleset& ruleset,
                    bool canonicalize_city_order) {
    require_civilization_inputs(elevation, climate, rivers, biome, features);
    const auto& civilization = ruleset.civilization_rules();
    if (!civilization.loaded || ancient_sites.width != elevation.width ||
        ancient_sites.height != elevation.height) {
        throw std::invalid_argument{"歷史古道缺少有效文明規則或上古選址"};
    }
    auto ordered_sites = ancient_sites.cities;
    std::ranges::sort(ordered_sites, {}, &CitySite::canonical_id);
    if (std::ranges::adjacent_find(ordered_sites, {}, &CitySite::canonical_id) !=
        ordered_sites.end()) {
        throw std::invalid_argument{"歷史古道遇到重複 canonical site id"};
    }
    for (const auto& site : ordered_sites) {
        if (site.tile.x < 0 || site.tile.y < 0 ||
            static_cast<std::uint32_t>(site.tile.x) >= elevation.width ||
            static_cast<std::uint32_t>(site.tile.y) >= elevation.height ||
            site.canonical_id >= elevation.meters.size() ||
            site.canonical_id !=
                static_cast<std::uint32_t>(static_cast<std::size_t>(site.tile.y) * elevation.width +
                                           static_cast<std::size_t>(site.tile.x))) {
            throw std::invalid_argument{"歷史古道 canonical site id 與座標不符"};
        }
    }

    auto tiles = make_base_tiles(elevation, climate, rivers, biome, features, definitions);
    AncientRoadOutput output;
    output.skipped_river_edges.assign(tiles.edges.size(), 0);
    if (ordered_sites.size() < 2) {
        output.edges = std::move(tiles.edges);
        return output;
    }
    auto [candidates, tree] = build_minimum_spanning_tree(tiles, ordered_sites, ruleset);
    static_cast<void>(candidates);
    std::map<std::uint32_t, std::size_t> input_rank;
    for (std::size_t index = 0; index < ancient_sites.cities.size(); ++index) {
        input_rank.emplace(ancient_sites.cities[index].canonical_id, index);
    }
    std::ranges::sort(tree, [&](const auto& lhs, const auto& rhs) {
        const auto lhs_pair = std::pair{ordered_sites[lhs.first].canonical_id,
                                        ordered_sites[lhs.second].canonical_id};
        const auto rhs_pair = std::pair{ordered_sites[rhs.first].canonical_id,
                                        ordered_sites[rhs.second].canonical_id};
        if (canonicalize_city_order) {
            return lhs_pair < rhs_pair;
        }
        return std::pair{std::min(input_rank.at(lhs_pair.first), input_rank.at(lhs_pair.second)),
                         std::max(input_rank.at(lhs_pair.first), input_rank.at(lhs_pair.second))} <
               std::pair{std::min(input_rank.at(rhs_pair.first), input_rank.at(rhs_pair.second)),
                         std::max(input_rank.at(rhs_pair.first), input_rank.at(rhs_pair.second))};
    });

    for (const auto& connection : tree) {
        const auto start = ordered_sites[connection.first].canonical_id;
        const auto goal = ordered_sites[connection.second].canonical_id;
        const auto path =
            find_engineering_path(tiles, rivers, start, goal, ruleset, civilization);
        for (std::size_t step = 1; step < path.size(); ++step) {
            const auto from = path[step - 1U];
            const auto to = path[step];
            const auto forward = directed_offset(from, to, tiles.width);
            const auto backward = directed_offset(to, from, tiles.width);
            const auto existing =
                tiles.edge_between(coordinate(from, tiles.width), coordinate(to, tiles.width));
            const auto* definition = ruleset.edge(existing);
            if (definition == nullptr) {
                throw std::runtime_error{"歷史古道遇到無效 EdgeId"};
            }
            if ((definition->flags & rules::kEdgeRiverFlag) != 0) {
                output.skipped_river_edges[forward] = 1;
                output.skipped_river_edges[backward] = 1;
                continue;
            }
            tiles.set_edge(coordinate(from, tiles.width), coordinate(to, tiles.width),
                           civilization.history.road_edge);
        }
        output.connections.push_back({ordered_sites[connection.first].canonical_id,
                                      ordered_sites[connection.second].canonical_id,
                                      connection.cost, false});
    }
    output.edges = std::move(tiles.edges);
    return output;
}

}  // namespace aetheria::worldgen::detail

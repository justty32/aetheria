#include "core/worldgen/region_late_stages.h"

#include "core/worldgen/civ_tiles.h"
#include "core/worldgen/portal_candidates.h"
#include "core/worldgen/road_path.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <vector>

namespace aetheria::worldgen {
namespace {

void install_road(world::RegionTiles& tiles, const RiverStageOutput& rivers,
                  std::span<const CitySite> cities, std::size_t portal,
                  const rules::Ruleset& ruleset, std::size_t road_tier) {
    const auto nearest = std::ranges::min_element(cities, [&](const CitySite& lhs,
                                                              const CitySite& rhs) {
        const auto distance = [&](const CitySite& city) {
            const auto portal_tile = detail::coordinate(portal, tiles.width);
            return std::pair{
                std::abs(static_cast<int>(portal_tile.x) - static_cast<int>(city.tile.x)) +
                    std::abs(static_cast<int>(portal_tile.y) - static_cast<int>(city.tile.y)),
                city.canonical_id};
        };
        return distance(lhs) < distance(rhs);
    });
    if (nearest == cities.end()) {
        throw std::runtime_error{"出境點補路時沒有城市"};
    }
    const auto& civilization = ruleset.civilization_rules();
    const auto path = detail::find_engineering_path(tiles, rivers, portal,
                                                    nearest->canonical_id, ruleset,
                                                    civilization);
    const auto road = civilization.road_edges.at(road_tier);
    for (std::size_t step = 1; step < path.size(); ++step) {
        const auto from = path[step - 1U];
        const auto to = path[step];
        const auto existing = tiles.edge_between(detail::coordinate(from, tiles.width),
                                                  detail::coordinate(to, tiles.width));
        const auto river = detail::underlying_river(civilization, existing, ruleset);
        const auto edge = river.has_value() ? detail::compound_edge(civilization, *river, road)
                                            : std::optional<rules::EdgeId>{road};
        if (!edge.has_value()) {
            throw std::runtime_error{"出境點補路缺少河級 × 道路級複合 def"};
        }
        tiles.set_edge(detail::coordinate(from, tiles.width),
                       detail::coordinate(to, tiles.width), *edge);
    }
}

}  // namespace

PortalStageOutput generate_portals(
    const QuantizedElevation& elevation, const ClimateStageOutput& climate,
    const RiverStageOutput& rivers, const BiomeStageOutput& biome,
    const HistoryStageOutput& history, const CityStageOutput& cities,
    const RoadStageOutput& roads, const RegionDefinitionIds& definitions,
    const rules::Ruleset& ruleset, std::uint32_t region_id, std::uint64_t stage_seed,
    const PortalGenerationConfig& config) {
    static_cast<void>(stage_seed);
    detail::require_civilization_inputs(elevation, climate, rivers, biome, history.features);
    if (!ruleset.civilization_rules().loaded || cities.width != elevation.width ||
        cities.height != elevation.height || cities.score.size() != elevation.meters.size() ||
        roads.width != elevation.width || roads.height != elevation.height ||
        roads.edges.size() != elevation.meters.size() * 4U || config.road_tier >= 3U) {
        throw std::invalid_argument{"出境點階段輸入或道路級別無效"};
    }
    auto tiles = detail::make_base_tiles(elevation, climate, rivers, biome, history.features,
                                         definitions);
    tiles.edges = roads.edges;
    auto output = PortalStageOutput{elevation.width, elevation.height, cities, roads.edges, {}};
    for (const auto& city : output.cities.cities) {
        tiles.settlement.at(city.canonical_id) = city.tier;
    }
    std::vector<std::uint8_t> occupied(tiles.tile_count());
    for (const auto& connection : ruleset.world_connections()) {
        if (connection.region_a != region_id && connection.region_b != region_id) {
            continue;
        }
        const bool endpoint_a = connection.region_a == region_id;
        std::size_t portal{};
        switch (connection.type) {
        case rules::WorldConnectionType::SeaRoute:
            portal = detail::resolve_sea_portal(tiles, output.cities, ruleset, occupied);
            break;
        case rules::WorldConnectionType::MountainPass:
            portal = detail::resolve_boundary_portal(tiles, ruleset, false, occupied);
            break;
        case rules::WorldConnectionType::Underground:
            portal = detail::resolve_boundary_portal(tiles, ruleset, true, occupied);
            break;
        case rules::WorldConnectionType::Teleport:
            portal =
                detail::resolve_teleport_portal(tiles, connection, endpoint_a, ruleset, occupied);
            break;
        }
        if (occupied.at(portal) != 0) {
            throw std::runtime_error{"出境點解析重複使用已佔用格"};
        }
        occupied[portal] = 1;
        install_road(tiles, rivers, output.cities.cities, portal, ruleset, config.road_tier);
        output.portals.push_back({detail::coordinate(portal, tiles.width), connection.id});
    }
    std::ranges::sort(output.portals, {}, [](const world::RegionPortal& portal) {
        return rules::value_of(portal.channel);
    });
    output.edges = std::move(tiles.edges);
    return output;
}

}  // namespace aetheria::worldgen

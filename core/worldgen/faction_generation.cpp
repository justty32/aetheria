#include "core/worldgen/region_late_stages.h"

#include "core/worldgen/civ_tiles.h"

#include <limits>
#include <stdexcept>

namespace aetheria::worldgen {

FactionStageOutput generate_factions(
    const QuantizedElevation& elevation, const ClimateStageOutput& climate,
    const RiverStageOutput& rivers, const BiomeStageOutput& biome,
    const HistoryStageOutput& history, const PortalStageOutput& portals,
    const RegionDefinitionIds& definitions, const rules::Ruleset& ruleset,
    std::uint64_t stage_seed, const FactionGenerationConfig& config) {
    static_cast<void>(stage_seed);
    detail::require_civilization_inputs(elevation, climate, rivers, biome, history.features);
    const auto& factions = ruleset.civilization_rules().factions;
    if (!ruleset.civilization_rules().loaded || factions.faction_count == 0 ||
        config.first_faction_id == 0 ||
        static_cast<std::uint32_t>(config.first_faction_id) + factions.faction_count - 1U >
            std::numeric_limits<std::uint16_t>::max() ||
        portals.width != elevation.width || portals.height != elevation.height ||
        portals.edges.size() != elevation.meters.size() * 4U) {
        throw std::invalid_argument{"勢力階段輸入或勢力 id 範圍無效"};
    }
    auto tiles = detail::make_base_tiles(elevation, climate, rivers, biome, history.features,
                                         definitions);
    tiles.edges = portals.edges;
    tiles.portals = portals.portals;
    for (const auto& city : portals.cities.cities) {
        tiles.settlement.at(city.canonical_id) = city.tier;
    }
    const auto selected = select_capitals(portals.cities.cities, factions.faction_count);
    FactionStageOutput output{elevation.width, elevation.height, {}, {}};
    output.capitals.reserve(selected.size());
    for (std::size_t index = 0; index < selected.size(); ++index) {
        output.capitals.push_back(
            {static_cast<world::FactionId>(config.first_faction_id + index), selected[index].tile});
    }
    output.owner = spread_influence(tiles, output.capitals, ruleset, factions);
    return output;
}

}  // namespace aetheria::worldgen

#include "core/worldgen/region_skeleton.h"

#include "core/worldgen/gen_grid.h"

#include <algorithm>
#include <stdexcept>

namespace aetheria::worldgen {

world::RegionTiles populate(const RegionSkeleton& skeleton, const RegionFastVariables& fast) {
    static_cast<void>(fast);
    const auto count = detail::checked_count(skeleton.elevation.width, skeleton.elevation.height);
    if (skeleton.elevation.meters.size() != count || skeleton.elevation.land.size() != count ||
        skeleton.climate.temperature_tenths.size() != count ||
        skeleton.rivers.moisture.size() != count || skeleton.rivers.downstream.size() != count ||
        skeleton.rivers.river_class.size() != count || skeleton.biome.terrain.size() != count ||
        skeleton.biome.relief.size() != count || skeleton.features.feature.size() != count ||
        skeleton.history.ancient_sites.width != skeleton.elevation.width ||
        skeleton.history.ancient_sites.height != skeleton.elevation.height ||
        skeleton.history.ancient_sites.score.size() != count ||
        skeleton.history.ancient_sites.bottleneck.size() != count ||
        skeleton.history.features.width != skeleton.elevation.width ||
        skeleton.history.features.height != skeleton.elevation.height ||
        skeleton.history.features.feature.size() != count ||
        skeleton.history.edges.size() != count * 4U ||
        skeleton.history.survivor.size() != count ||
        skeleton.history.skipped_river_edges.size() != count * 4U ||
        skeleton.cities.score.size() != count || skeleton.cities.bottleneck.size() != count ||
        skeleton.roads.edges.size() != count * 4U ||
        skeleton.roads.usage.size() != count * 4U ||
        skeleton.portals.width != skeleton.elevation.width ||
        skeleton.portals.height != skeleton.elevation.height ||
        skeleton.portals.cities.score.size() != count ||
        skeleton.portals.edges.size() != count * 4U ||
        skeleton.factions.width != skeleton.elevation.width ||
        skeleton.factions.height != skeleton.elevation.height ||
        skeleton.factions.owner.size() != count ||
        skeleton.elevation.width > static_cast<std::uint32_t>(INT16_MAX) ||
        skeleton.elevation.height > static_cast<std::uint32_t>(INT16_MAX)) {
        throw std::invalid_argument{"RegionSkeleton 尺寸不一致"};
    }
    world::RegionTiles tiles{skeleton.elevation.width, skeleton.elevation.height};
    tiles.elevation = skeleton.elevation.meters;
    for (std::size_t index = 0; index < count; ++index) {
        tiles.base[index] = skeleton.biome.terrain[index];
        tiles.relief[index] = skeleton.biome.relief[index];
        tiles.feature[index] = skeleton.history.features.feature[index];
        tiles.temperature[index] = static_cast<std::uint8_t>(std::clamp<std::int32_t>(
            (static_cast<std::int32_t>(skeleton.climate.temperature_tenths[index]) + 500) * 255 /
                1000,
            0, UINT8_MAX));
        tiles.moisture[index] = static_cast<std::uint8_t>(skeleton.rivers.moisture[index] / 257U);
    }
    tiles.edges = skeleton.portals.edges;
    tiles.owner = skeleton.factions.owner;
    tiles.portals = skeleton.portals.portals;
    for (const auto& city : skeleton.portals.cities.cities) {
        const auto index = tiles.index_of(city.tile);
        if (index != city.canonical_id || tiles.settlement[index] != world::SettlementTier::None) {
            throw std::runtime_error{"城市 canonical id 或位置重複"};
        }
        tiles.settlement[index] = city.tier;
    }
    return tiles;
}

}  // namespace aetheria::worldgen

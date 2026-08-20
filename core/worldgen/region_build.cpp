#include "core/worldgen/region_skeleton.h"

#include "core/worldgen/gen_grid.h"
#include "core/worldgen/gen_stage_ids.h"
#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace aetheria::worldgen {
namespace {

template <typename Id, typename Finder>
[[nodiscard]] Id require_definition(Finder&& finder, std::string id) {
    const auto found = finder(id);
    if (!found.has_value()) {
        throw std::invalid_argument{"Region 生成缺少必要 definition：" + id};
    }
    return *found;
}

}  // namespace

RegionBuildResult build_skeleton(const RegionSlowVariables& slow, std::uint64_t world_seed,
                                 const rules::Ruleset& ruleset,
                                 const RegionGenerationConfig& config) {
    RegionDefinitionIds definitions{
        require_definition<rules::TerrainId>(
            [&](const std::string& id) { return ruleset.find_terrain(id); }, "terrain.grassland"),
        require_definition<rules::TerrainId>(
            [&](const std::string& id) { return ruleset.find_terrain(id); }, "terrain.ocean"),
        require_definition<rules::ReliefId>(
            [&](const std::string& id) { return ruleset.find_relief(id); }, "relief.plain"),
        require_definition<rules::FeatureId>(
            [&](const std::string& id) { return ruleset.find_feature(id); }, "feature.none"),
        require_definition<rules::FeatureId>(
            [&](const std::string& id) { return ruleset.find_feature(id); }, "feature.forest"),
        require_definition<rules::FeatureId>(
            [&](const std::string& id) { return ruleset.find_feature(id); }, "feature.mine"),
        require_definition<rules::FeatureId>(
            [&](const std::string& id) { return ruleset.find_feature(id); }, "feature.oasis"),
        require_definition<rules::FeatureId>(
            [&](const std::string& id) { return ruleset.find_feature(id); }, "feature.landmark"),
        require_definition<rules::EdgeId>(
            [&](const std::string& id) { return ruleset.find_edge(id); }, "edge.none"),
        require_definition<rules::EdgeId>(
            [&](const std::string& id) { return ruleset.find_edge(id); }, "edge.stream"),
        require_definition<rules::EdgeId>(
            [&](const std::string& id) { return ruleset.find_edge(id); }, "edge.river"),
        require_definition<rules::EdgeId>(
            [&](const std::string& id) { return ruleset.find_edge(id); }, "edge.great_river")};

    auto plates = generate_plates(
        slow, derive_region_stage_seed(world_seed, slow.region_id, detail::kPlateStageId),
        config.plates);
    auto height = generate_height(
        plates, derive_region_stage_seed(world_seed, slow.region_id, detail::kHeightStageId),
        config.height);
    auto erosion = erode_height(
        height, derive_region_stage_seed(world_seed, slow.region_id, detail::kErosionStageId),
        config.erosion);
    auto elevation = quantize_elevation(erosion);
    auto climate = generate_climate(
        slow, elevation,
        derive_region_stage_seed(world_seed, slow.region_id, detail::kClimateStageId),
        config.climate);
    auto rivers = generate_rivers(
        elevation, climate,
        derive_region_stage_seed(world_seed, slow.region_id, detail::kRiverStageId),
        config.rivers);
    auto biome = generate_biomes(
        elevation, climate, rivers, ruleset, definitions,
        derive_region_stage_seed(world_seed, slow.region_id, detail::kBiomeStageId), config.biome);
    auto features = generate_features(
        plates, elevation, climate, rivers, biome, definitions,
        derive_region_stage_seed(world_seed, slow.region_id, detail::kFeatureStageId),
        config.features);
    auto history = generate_history(
        elevation, climate, rivers, biome, features, definitions, ruleset,
        derive_region_stage_seed(world_seed, slow.region_id, detail::kHistoryStageId),
        config.history);
    auto cities = generate_cities(
        elevation, climate, rivers, biome, history, ruleset,
        derive_region_stage_seed(world_seed, slow.region_id, detail::kCityStageId), config.cities);
    auto roads = generate_roads(
        elevation, climate, rivers, biome, history, cities, definitions, ruleset,
        derive_region_stage_seed(world_seed, slow.region_id, detail::kRoadStageId), config.roads);
    RegionSkeleton skeleton{elevation, climate, rivers, biome, features, history, cities, roads,
                            definitions};
    return {std::move(plates),   std::move(height),   std::move(erosion),
            std::move(climate),  std::move(rivers),   std::move(biome),
            std::move(features), std::move(history),  std::move(cities),
            std::move(roads),    std::move(skeleton)};
}

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
    tiles.edges = skeleton.roads.edges;
    for (const auto& city : skeleton.cities.cities) {
        const auto index = tiles.index_of(city.tile);
        if (index != city.canonical_id || tiles.settlement[index] != world::SettlementTier::None) {
            throw std::runtime_error{"城市 canonical id 或位置重複"};
        }
        tiles.settlement[index] = city.tier;
    }
    return tiles;
}

}  // namespace aetheria::worldgen

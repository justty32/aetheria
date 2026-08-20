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
        require_definition<rules::FeatureId>(
            [&](const std::string& id) { return ruleset.find_feature(id); },
            "feature.ancient_foundation"),
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
        plates, elevation, climate, rivers, biome, definitions, ruleset,
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
    auto portals = generate_portals(
        elevation, climate, rivers, biome, history, cities, roads, definitions, ruleset,
        slow.region_id,
        derive_region_stage_seed(world_seed, slow.region_id, detail::kPortalStageId),
        config.portals);
    auto factions = generate_factions(
        elevation, climate, rivers, biome, history, portals, definitions, ruleset,
        derive_region_stage_seed(world_seed, slow.region_id, detail::kFactionStageId),
        config.factions);
    RegionSkeleton skeleton{elevation, climate, rivers, biome, features, history, cities,
                            roads,     portals, factions, definitions};
    return {std::move(plates),   std::move(height),   std::move(erosion),
            std::move(climate),  std::move(rivers),   std::move(biome),
            std::move(features), std::move(history),  std::move(cities),
            std::move(roads),    std::move(portals),  std::move(factions),
            std::move(skeleton)};
}

}  // namespace aetheria::worldgen

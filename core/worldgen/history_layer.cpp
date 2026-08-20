#include "core/worldgen/region_civ_stages.h"

#include "core/worldgen/city_selection.h"
#include "core/worldgen/feature_placement.h"
#include "core/worldgen/history_roads.h"

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace aetheria::worldgen {

HistoryStageOutput generate_history(const QuantizedElevation& elevation,
                                    const ClimateStageOutput& climate,
                                    const RiverStageOutput& rivers,
                                    const BiomeStageOutput& biome,
                                    const FeatureStageOutput& features,
                                    const RegionDefinitionIds& definitions,
                                    const rules::Ruleset& ruleset, std::uint64_t stage_seed,
                                    const HistoryGenerationConfig& config) {
    const auto& civilization = ruleset.civilization_rules();
    if (!civilization.loaded) {
        throw std::invalid_argument{"Ruleset 缺少 civilization.toml 歷史規則"};
    }
    auto ancient_sites = score_city_sites(elevation, climate, rivers, biome, features, ruleset,
                                          civilization.history.scoring_weights);
    const auto target = civilization.history.ancient_site_count;
    const auto city_count = std::min(civilization.history.ancient_city_count, target);
    const auto town_count = std::min<std::uint16_t>(
        civilization.history.ancient_town_count,
        static_cast<std::uint16_t>(target - city_count));
    ancient_sites.cities = detail::select_city_sites(
        elevation, ancient_sites, stage_seed,
        {target, city_count, town_count, civilization.history.minimum_spacing,
         config.minimum_score_bias});
    return generate_history_from_sites(elevation, climate, rivers, biome, features,
                                       std::move(ancient_sites), definitions, ruleset);
}

HistoryStageOutput
generate_history_from_sites(const QuantizedElevation& elevation,
                            const ClimateStageOutput& climate,
                            const RiverStageOutput& rivers, const BiomeStageOutput& biome,
                            const FeatureStageOutput& features, CityStageOutput ancient_sites,
                            const RegionDefinitionIds& definitions,
                            const rules::Ruleset& ruleset, bool canonicalize_city_order) {
    const auto& civilization = ruleset.civilization_rules();
    const auto count = elevation.meters.size();
    if (!civilization.loaded || ancient_sites.width != elevation.width ||
        ancient_sites.height != elevation.height || ancient_sites.score.size() != count ||
        ancient_sites.bottleneck.size() != count) {
        throw std::invalid_argument{"歷史層缺少有效上古評分或文明規則"};
    }
    auto road_output = detail::build_ancient_roads(
        elevation, climate, rivers, biome, features, ancient_sites, definitions, ruleset,
        canonicalize_city_order);
    FeatureStageOutput history_features = features;
    std::vector<std::uint8_t> survivor(count, 0);
    std::vector<std::size_t> catastrophe_order(ancient_sites.cities.size());
    std::iota(catastrophe_order.begin(), catastrophe_order.end(), 0U);
    std::ranges::sort(catastrophe_order, [&](std::size_t lhs, std::size_t rhs) {
        const auto& lhs_site = ancient_sites.cities[lhs];
        const auto& rhs_site = ancient_sites.cities[rhs];
        return lhs_site.score != rhs_site.score ? lhs_site.score > rhs_site.score
                                                : lhs_site.canonical_id < rhs_site.canonical_id;
    });
    const auto survivor_count =
        catastrophe_order.size() * civilization.history.survivor_percent / 100U;
    for (std::size_t rank = 0; rank < catastrophe_order.size(); ++rank) {
        const auto& site = ancient_sites.cities[catastrophe_order[rank]];
        const auto tier = static_cast<std::uint8_t>(site.tier);
        if (site.canonical_id >= count ||
            tier < static_cast<std::uint8_t>(world::SettlementTier::Village) ||
            tier > static_cast<std::uint8_t>(world::SettlementTier::City)) {
            throw std::invalid_argument{"歷史層遇到無效上古選址"};
        }
        if (rank < survivor_count) {
            survivor[site.canonical_id] = 1;
            continue;
        }
        const auto tier_index = static_cast<std::size_t>(tier) - 1U;
        const auto ruin = civilization.history.ruin_features[tier_index];
        detail::require_feature_terrain(ruleset, ruin, biome.terrain[site.canonical_id]);
        history_features.feature[site.canonical_id] = ruin;
    }
    return {std::move(ancient_sites), std::move(history_features), std::move(road_output.edges),
            std::move(survivor), std::move(road_output.connections),
            std::move(road_output.skipped_river_edges)};
}

}  // namespace aetheria::worldgen

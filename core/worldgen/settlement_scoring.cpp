#include "core/worldgen/region_civ_stages.h"

#include "core/worldgen/city_scoring.h"
#include "core/worldgen/civ_tiles.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace aetheria::worldgen {

CityStageOutput score_city_sites(const QuantizedElevation& elevation,
                                 const ClimateStageOutput& climate,
                                 const RiverStageOutput& rivers,
                                 const BiomeStageOutput& biome,
                                 const FeatureStageOutput& features,
                                 const rules::Ruleset& ruleset,
                                 const rules::SettlementScoringWeights& weights) {
    detail::require_civilization_inputs(elevation, climate, rivers, biome, features);
    const auto& civilization = ruleset.civilization_rules();
    if (!civilization.loaded) {
        throw std::invalid_argument{"Ruleset 缺少 civilization.toml"};
    }
    const auto count = elevation.meters.size();
    CityStageOutput output{elevation.width, elevation.height, {}, {}, {}};
    output.score.assign(count, std::numeric_limits<std::int32_t>::min());
    output.bottleneck.resize(count);
    const auto open_ocean = detail::ocean_connected_to_boundary(elevation);
    const auto no_feature = ruleset.find_feature("feature.none");
    if (!no_feature.has_value()) {
        throw std::runtime_error{"城市階段缺少 feature.none"};
    }
    for (std::size_t index = 0; index < count; ++index) {
        if (elevation.land[index] == 0) {
            continue;
        }
        output.bottleneck[index] = detail::local_bottleneck_score(
            elevation, index, civilization.bottleneck_radius);
        std::int64_t score{};
        bool freshwater = rivers.river_class[index] != 0;
        bool harbor{};
        std::uint16_t defenses{};
        for (const auto next : detail::neighbors(index, elevation.width, elevation.height)) {
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
            score += weights.freshwater;
        }
        if (harbor) {
            score += weights.harbor;
        }
        score += static_cast<std::int64_t>(defenses) * weights.defense;
        const auto center = detail::coordinate(index, elevation.width);
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
        score += static_cast<std::int64_t>(farmland) * weights.farmland;
        score += static_cast<std::int64_t>(resources) * weights.resource;
        score += static_cast<std::int64_t>(output.bottleneck[index]) *
                 weights.bottleneck;
        if (climate.temperature_tenths[index] <= 20 ||
            climate.temperature_tenths[index] >= 350 || rivers.moisture[index] <= 8000 ||
            biome.terrain[index] == civilization.swamp_terrain) {
            score += weights.extreme_climate_penalty;
        }
        if (elevation.meters[index] >= civilization.high_elevation_threshold) {
            score += weights.high_elevation_penalty;
        }
        output.score[index] = static_cast<std::int32_t>(
            std::clamp<std::int64_t>(score, std::numeric_limits<std::int32_t>::min(),
                                     std::numeric_limits<std::int32_t>::max()));
    }
    return output;
}

}  // namespace aetheria::worldgen

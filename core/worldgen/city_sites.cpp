#include "core/worldgen/city_scoring.h"
#include "core/worldgen/civ_tiles.h"
#include "core/worldgen/region_generator.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace aetheria::worldgen {
namespace {

using detail::coordinate;
using detail::neighbors;
using detail::local_bottleneck_score;
using detail::ocean_connected_to_boundary;

[[nodiscard]] std::uint32_t manhattan(world::RegionXY lhs, world::RegionXY rhs) noexcept {
    return static_cast<std::uint32_t>(std::abs(static_cast<int>(lhs.x) - static_cast<int>(rhs.x)) +
                                      std::abs(static_cast<int>(lhs.y) - static_cast<int>(rhs.y)));
}

}  // namespace

CityStageOutput generate_cities(const QuantizedElevation& elevation,
                                const ClimateStageOutput& climate, const RiverStageOutput& rivers,
                                const BiomeStageOutput& biome, const FeatureStageOutput& features,
                                const rules::Ruleset& ruleset, std::uint64_t stage_seed,
                                const CityGenerationConfig& config) {
    detail::require_civilization_inputs(elevation, climate, rivers, biome, features);
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

}  // namespace aetheria::worldgen

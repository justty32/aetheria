#include "core/worldgen/region_climate_stages.h"

#include "core/worldgen/feature_placement.h"
#include "core/worldgen/gen_grid.h"
#include "core/worldgen/region_seed.h"
#include "core/worldgen/region_skeleton.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace aetheria::worldgen {

FeatureStageOutput generate_features(const PlateStageOutput& plates,
                                     const QuantizedElevation& elevation,
                                     const ClimateStageOutput& climate,
                                     const RiverStageOutput& rivers, const BiomeStageOutput& biome,
                                     const RegionDefinitionIds& definitions,
                                     const rules::Ruleset& ruleset, std::uint64_t stage_seed,
                                     const FeatureGenerationConfig& config) {
    const auto count = detail::checked_count(elevation.width, elevation.height);
    if (plates.width != elevation.width || plates.height != elevation.height ||
        plates.boundary_effect.size() != count || climate.temperature_tenths.size() != count ||
        rivers.moisture.size() != count || biome.terrain.size() != count ||
        biome.relief.size() != count) {
        throw std::invalid_argument{"地物階段輸入尺寸不一致"};
    }
    for (const auto feature : std::array{definitions.landmark, definitions.oasis, definitions.mine,
                                         definitions.forest}) {
        detail::validate_land_feature(ruleset, feature);
    }
    FeatureStageOutput output{elevation.width, elevation.height, {}};
    output.feature.assign(count, definitions.no_feature);
    auto priority = [&](std::size_t index) {
        return splitmix64(stage_seed ^
                          (static_cast<std::uint64_t>(index) * UINT64_C(0xD6E8FEB86659FD93)));
    };
    for (std::size_t index = 0; index < count; ++index) {
        if (elevation.land[index] == 0) {
            continue;
        }
        const auto terrain = biome.terrain[index];
        const auto random = priority(index);
        if (static_cast<std::uint16_t>(random) < config.landmark_chance &&
            detail::feature_accepts_terrain(ruleset, definitions.landmark, terrain)) {
            output.feature[index] = definitions.landmark;
            continue;
        }
        if (rivers.moisture[index] < 12500 &&
            static_cast<std::uint16_t>(random >> 16U) < config.oasis_chance &&
            detail::feature_accepts_terrain(ruleset, definitions.oasis, terrain)) {
            output.feature[index] = definitions.oasis;
            continue;
        }
        if (std::abs(static_cast<int>(plates.boundary_effect[index])) >= 300 &&
            biome.relief[index] != definitions.plain &&
            static_cast<std::uint16_t>(random >> 32U) < config.mine_chance &&
            detail::feature_accepts_terrain(ruleset, definitions.mine, terrain)) {
            output.feature[index] = definitions.mine;
            continue;
        }
        if (climate.temperature_tenths[index] <= 40 || rivers.moisture[index] <= 12000 ||
            !detail::feature_accepts_terrain(ruleset, definitions.forest, terrain)) {
            continue;
        }
        const auto forest_chance =
            static_cast<std::uint16_t>(static_cast<std::uint32_t>(rivers.moisture[index]) *
                                       config.forest_density_scale / UINT16_MAX);
        if (static_cast<std::uint16_t>(random >> 48U) >= forest_chance) {
            continue;
        }
        bool local_priority = true;
        for (const auto neighbor : detail::neighbors(index, elevation.width, elevation.height)) {
            if (neighbor < count && priority(neighbor) > random) {
                local_priority = false;
                break;
            }
        }
        if (local_priority) {
            output.feature[index] = definitions.forest;
        }
    }
    return output;
}

}  // namespace aetheria::worldgen

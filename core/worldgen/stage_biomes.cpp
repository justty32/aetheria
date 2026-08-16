#include "core/worldgen/region_climate_stages.h"

#include "core/worldgen/gen_grid.h"
#include "core/worldgen/region_seed.h"
#include "core/worldgen/region_skeleton.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace aetheria::worldgen {

BiomeStageOutput generate_biomes(const QuantizedElevation& elevation,
                                 const ClimateStageOutput& climate, const RiverStageOutput& rivers,
                                 const rules::Ruleset& ruleset,
                                 const RegionDefinitionIds& definitions, std::uint64_t stage_seed,
                                 const BiomeGenerationConfig& config) {
    static_cast<void>(stage_seed);
    const auto count = detail::checked_count(elevation.width, elevation.height);
    if (elevation.meters.size() != count || elevation.land.size() != count ||
        climate.width != elevation.width || climate.height != elevation.height ||
        climate.temperature_tenths.size() != count || rivers.width != elevation.width ||
        rivers.height != elevation.height || rivers.moisture.size() != count ||
        ruleset.biome_rules().empty()) {
        throw std::invalid_argument{"biome 階段輸入尺寸不一致或缺少 biomes.toml 規則"};
    }
    BiomeStageOutput output{elevation.width, elevation.height, {}, {}};
    output.terrain.resize(count);
    output.relief.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
        if (elevation.land[index] == 0) {
            output.terrain[index] = definitions.ocean;
            output.relief[index] = definitions.plain;
            continue;
        }
        const auto x = static_cast<std::uint32_t>(index % elevation.width);
        const auto y = static_cast<std::uint32_t>(index / elevation.width);
        auto minimum = elevation.meters[index];
        auto maximum = elevation.meters[index];
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const auto nx = static_cast<int>(x) + dx;
                const auto ny = static_cast<int>(y) + dy;
                if (nx < 0 || ny < 0 || nx >= static_cast<int>(elevation.width) ||
                    ny >= static_cast<int>(elevation.height)) {
                    continue;
                }
                const auto neighbor =
                    static_cast<std::size_t>(ny) * elevation.width + static_cast<std::size_t>(nx);
                minimum = std::min(minimum, elevation.meters[neighbor]);
                maximum = std::max(maximum, elevation.meters[neighbor]);
            }
        }
        const auto ruggedness = static_cast<std::uint16_t>(maximum - minimum);
        const auto temperature = static_cast<std::int16_t>(std::clamp<std::int32_t>(
            static_cast<std::int32_t>(climate.temperature_tenths[index]) +
                config.temperature_bias_tenths,
            std::numeric_limits<std::int16_t>::min(), std::numeric_limits<std::int16_t>::max()));
        const auto moisture = static_cast<std::uint16_t>(std::clamp<std::int32_t>(
            static_cast<std::int32_t>(rivers.moisture[index]) + config.moisture_bias, 0,
            UINT16_MAX));
        bool matched{};
        for (const auto& rule : ruleset.biome_rules()) {
            if (rule.fallback ||
                (temperature >= rule.min_temperature_tenths &&
                 temperature <= rule.max_temperature_tenths && moisture >= rule.min_moisture &&
                 moisture <= rule.max_moisture && elevation.meters[index] >= rule.min_elevation &&
                 elevation.meters[index] <= rule.max_elevation &&
                 ruggedness >= rule.min_ruggedness && ruggedness <= rule.max_ruggedness)) {
                output.terrain[index] = rule.terrain;
                output.relief[index] = rule.relief;
                matched = true;
                break;
            }
        }
        if (!matched) {
            throw std::runtime_error{"BiomeRule 沒有 fallback 命中"};
        }
    }
    return output;
}

FeatureStageOutput
generate_features(const PlateStageOutput& plates, const QuantizedElevation& elevation,
                  const ClimateStageOutput& climate, const RiverStageOutput& rivers,
                  const BiomeStageOutput& biome, const RegionDefinitionIds& definitions,
                  std::uint64_t stage_seed, const FeatureGenerationConfig& config) {
    const auto count = detail::checked_count(elevation.width, elevation.height);
    if (plates.width != elevation.width || plates.height != elevation.height ||
        plates.boundary_effect.size() != count || climate.temperature_tenths.size() != count ||
        rivers.moisture.size() != count || biome.terrain.size() != count ||
        biome.relief.size() != count) {
        throw std::invalid_argument{"地物階段輸入尺寸不一致"};
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
        const auto random = priority(index);
        if (static_cast<std::uint16_t>(random) < config.landmark_chance) {
            output.feature[index] = definitions.landmark;
            continue;
        }
        if (rivers.moisture[index] < 12500 &&
            static_cast<std::uint16_t>(random >> 16U) < config.oasis_chance) {
            output.feature[index] = definitions.oasis;
            continue;
        }
        if (std::abs(static_cast<int>(plates.boundary_effect[index])) >= 300 &&
            biome.relief[index] != definitions.plain &&
            static_cast<std::uint16_t>(random >> 32U) < config.mine_chance) {
            output.feature[index] = definitions.mine;
            continue;
        }
        if (climate.temperature_tenths[index] <= 40 || rivers.moisture[index] <= 12000) {
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

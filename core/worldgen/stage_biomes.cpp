#include "core/worldgen/region_climate_stages.h"

#include "core/worldgen/biome_classification.h"
#include "core/worldgen/gen_grid.h"
#include "core/worldgen/region_skeleton.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace aetheria::worldgen {
namespace {

template <typename Value>
[[nodiscard]] std::int64_t normalized_distance(Value value, Value target,
                                               std::uint16_t scale) noexcept {
    const auto distance = std::abs(static_cast<std::int64_t>(value) -
                                   static_cast<std::int64_t>(target));
    return distance * INT64_C(65536) / scale;
}

[[nodiscard]] std::int64_t terrain_score(const rules::TerrainRule& rule,
                                         TerrainClassificationInput input) noexcept {
    auto penalty = std::int64_t{};
    auto axes = std::int64_t{};
    if (rule.temperature_scale_tenths != 0) {
        penalty += normalized_distance(input.temperature_tenths,
                                       rule.temperature_target_tenths,
                                       rule.temperature_scale_tenths);
        ++axes;
    }
    if (rule.moisture_scale != 0) {
        penalty += normalized_distance(input.moisture, rule.moisture_target, rule.moisture_scale);
        ++axes;
    }
    if (rule.elevation_scale != 0) {
        penalty +=
            normalized_distance(input.elevation, rule.elevation_target, rule.elevation_scale);
        ++axes;
    }
    return static_cast<std::int64_t>(rule.score_bias) * INT64_C(65536) - penalty / axes;
}

}  // namespace

rules::TerrainId classify_terrain(const rules::Ruleset& ruleset,
                                  TerrainClassificationInput input) {
    const auto rules = ruleset.terrain_rules();
    if (rules.empty()) {
        throw std::runtime_error{"TerrainRule 計分表為空"};
    }
    auto best_index = std::size_t{};
    auto best_score = terrain_score(rules.front(), input);
    for (std::size_t index = 1; index < rules.size(); ++index) {
        const auto score = terrain_score(rules[index], input);
        if (score > best_score) {
            best_score = score;
            best_index = index;
        }
    }
    return rules[best_index].terrain;
}

rules::ReliefId classify_relief(const rules::Ruleset& ruleset,
                                ReliefClassificationInput input) {
    for (const auto& rule : ruleset.relief_rules()) {
        if (rule.fallback ||
            (input.elevation >= rule.min_elevation && input.elevation <= rule.max_elevation &&
             input.ruggedness >= rule.min_ruggedness &&
             input.ruggedness <= rule.max_ruggedness)) {
            return rule.relief;
        }
    }
    throw std::runtime_error{"ReliefRule 沒有 fallback 命中"};
}

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
        ruleset.terrain_rules().empty() || ruleset.relief_rules().empty()) {
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
        output.relief[index] = classify_relief(
            ruleset, ReliefClassificationInput{elevation.meters[index], ruggedness});
        const auto temperature = static_cast<std::int16_t>(std::clamp<std::int32_t>(
            static_cast<std::int32_t>(climate.temperature_tenths[index]) +
                config.temperature_bias_tenths,
            std::numeric_limits<std::int16_t>::min(), std::numeric_limits<std::int16_t>::max()));
        const auto moisture = static_cast<std::uint16_t>(std::clamp<std::int32_t>(
            static_cast<std::int32_t>(rivers.moisture[index]) + config.moisture_bias, 0,
            UINT16_MAX));
        output.terrain[index] = classify_terrain(
            ruleset, TerrainClassificationInput{temperature, moisture, elevation.meters[index]});
    }
    return output;
}

}  // namespace aetheria::worldgen

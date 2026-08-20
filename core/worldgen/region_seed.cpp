#include "core/worldgen/region_seed.h"

#include "core/worldgen/gen_hash.h"
#include "core/worldgen/gen_stage_ids.h"
#include "core/worldgen/region_config.h"

namespace aetheria::worldgen {

std::uint64_t splitmix64(std::uint64_t value) noexcept {
    value += UINT64_C(0x9E3779B97F4A7C15);
    value = (value ^ (value >> 30U)) * UINT64_C(0xBF58476D1CE4E5B9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94D049BB133111EB);
    return value ^ (value >> 31U);
}

std::uint64_t derive_stage_seed(std::uint64_t world_seed, std::uint64_t stage_id) noexcept {
    return splitmix64(world_seed ^ stage_id);
}

std::uint64_t derive_region_seed(std::uint64_t world_seed, std::uint32_t region_id) noexcept {
    return splitmix64(world_seed ^ detail::kRegionSalt ^ region_id);
}

std::uint64_t derive_region_stage_seed(std::uint64_t world_seed, std::uint32_t region_id,
                                       std::uint64_t stage_id) noexcept {
    return splitmix64(derive_stage_seed(world_seed, stage_id) ^
                      derive_region_seed(world_seed, region_id));
}

GenerationParameterHashes
generation_parameter_hashes(const RegionGenerationConfig& config) noexcept {
    GenerationParameterHashes result;
    auto begin_group = [] { return UINT64_C(14695981039346656037); };

    result.groups[0] = begin_group();
    detail::hash_scalar(result.groups[0], config.plates.min_count);
    detail::hash_scalar(result.groups[0], config.plates.max_count);
    result.groups[1] = begin_group();
    detail::hash_scalar(result.groups[1], config.height.noise_octaves);
    detail::hash_scalar(result.groups[1], config.height.target_land_percent);
    result.groups[2] = begin_group();
    detail::hash_scalar(result.groups[2], config.erosion.iterations);
    detail::hash_double(result.groups[2], config.erosion.talus);
    detail::hash_double(result.groups[2], config.erosion.transfer_fraction);
    result.groups[3] = begin_group();
    detail::hash_scalar(result.groups[3], config.climate.lapse_tenths_per_km);
    detail::hash_scalar(result.groups[3], config.climate.air_decay);
    detail::hash_scalar(result.groups[3], config.climate.uplift_rain);
    result.groups[4] = begin_group();
    detail::hash_scalar(result.groups[4], config.rivers.stream_threshold);
    detail::hash_scalar(result.groups[4], config.rivers.river_threshold);
    detail::hash_scalar(result.groups[4], config.rivers.great_river_threshold);
    detail::hash_scalar(result.groups[4], config.rivers.moisture_bonus);
    result.groups[5] = begin_group();
    detail::hash_scalar(result.groups[5], config.biome.temperature_bias_tenths);
    detail::hash_scalar(result.groups[5], config.biome.moisture_bias);
    result.groups[6] = begin_group();
    detail::hash_scalar(result.groups[6], config.features.forest_density_scale);
    detail::hash_scalar(result.groups[6], config.features.mine_chance);
    detail::hash_scalar(result.groups[6], config.features.oasis_chance);
    detail::hash_scalar(result.groups[6], config.features.landmark_chance);
    result.groups[7] = begin_group();
    detail::hash_scalar(result.groups[7], config.history.minimum_score_bias);
    result.groups[8] = begin_group();
    detail::hash_scalar(result.groups[8], config.cities.minimum_score_bias);
    result.groups[9] = begin_group();
    detail::hash_scalar(result.groups[9], config.roads.loop_percent_override);
    result.groups[10] = begin_group();
    detail::hash_scalar(result.groups[10], config.portals.road_tier);
    result.groups[11] = begin_group();
    detail::hash_scalar(result.groups[11], config.factions.first_faction_id);
    return result;
}

}  // namespace aetheria::worldgen

#include "core/worldgen/region_diagnostics.h"

#include "core/worldgen/gen_hash.h"

#include <cstdint>

namespace aetheria::worldgen {

std::uint64_t hash_stage(const PlateStageOutput& stage) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    detail::hash_scalar(hash, stage.width);
    detail::hash_scalar(hash, stage.height);
    detail::hash_scalar(hash, static_cast<std::uint64_t>(stage.plates.size()));
    for (const auto& plate : stage.plates) {
        detail::hash_scalar(hash, plate.x);
        detail::hash_scalar(hash, plate.y);
        detail::hash_scalar(hash, static_cast<std::uint8_t>(plate.is_oceanic));
        detail::hash_scalar(hash, plate.drift_x);
        detail::hash_scalar(hash, plate.drift_y);
        detail::hash_scalar(hash, plate.base_elevation);
    }
    detail::hash_vector(hash, stage.plate_index);
    detail::hash_vector(hash, stage.boundary_type);
    detail::hash_vector(hash, stage.boundary_effect);
    return hash;
}

std::uint64_t hash_stage(const HeightStageOutput& stage) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    detail::hash_scalar(hash, stage.width);
    detail::hash_scalar(hash, stage.height);
    detail::hash_double_vector(hash, stage.elevation);
    detail::hash_vector(hash, stage.land);
    detail::hash_double(hash, stage.sea_level);
    return hash;
}

std::uint64_t hash_stage(const ErosionStageOutput& stage) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    detail::hash_scalar(hash, stage.width);
    detail::hash_scalar(hash, stage.height);
    detail::hash_double_vector(hash, stage.elevation);
    detail::hash_vector(hash, stage.land);
    detail::hash_double(hash, stage.sea_level);
    return hash;
}

std::uint64_t hash_stage(const ClimateStageOutput& stage) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    detail::hash_scalar(hash, stage.width);
    detail::hash_scalar(hash, stage.height);
    detail::hash_vector(hash, stage.temperature_tenths);
    detail::hash_vector(hash, stage.moisture);
    detail::hash_vector(hash, stage.prevailing_wind_x);
    return hash;
}

std::uint64_t hash_stage(const RiverStageOutput& stage) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    detail::hash_scalar(hash, stage.width);
    detail::hash_scalar(hash, stage.height);
    detail::hash_vector(hash, stage.filled_elevation);
    detail::hash_vector(hash, stage.downstream);
    detail::hash_vector(hash, stage.flow);
    detail::hash_vector(hash, stage.river_class);
    detail::hash_vector(hash, stage.moisture);
    detail::hash_vector(hash, stage.lake);
    return hash;
}

std::uint64_t hash_stage(const BiomeStageOutput& stage) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    detail::hash_scalar(hash, stage.width);
    detail::hash_scalar(hash, stage.height);
    detail::hash_vector(hash, stage.terrain);
    detail::hash_vector(hash, stage.relief);
    return hash;
}

std::uint64_t hash_stage(const FeatureStageOutput& stage) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    detail::hash_scalar(hash, stage.width);
    detail::hash_scalar(hash, stage.height);
    detail::hash_vector(hash, stage.feature);
    return hash;
}

std::uint64_t hash_stage(const CityStageOutput& stage) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    detail::hash_scalar(hash, stage.width);
    detail::hash_scalar(hash, stage.height);
    detail::hash_vector(hash, stage.score);
    detail::hash_vector(hash, stage.bottleneck);
    detail::hash_scalar(hash, static_cast<std::uint64_t>(stage.cities.size()));
    for (const auto& city : stage.cities) {
        detail::hash_scalar(hash, city.canonical_id);
        detail::hash_scalar(hash, city.tile.x);
        detail::hash_scalar(hash, city.tile.y);
        detail::hash_scalar(hash, city.score);
        detail::hash_scalar(hash, city.tier);
        detail::hash_scalar(hash, city.minimum_spacing);
    }
    return hash;
}

std::uint64_t hash_stage(const RoadStageOutput& stage) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    detail::hash_scalar(hash, stage.width);
    detail::hash_scalar(hash, stage.height);
    detail::hash_vector(hash, stage.edges);
    detail::hash_vector(hash, stage.usage);
    detail::hash_scalar(hash, static_cast<std::uint64_t>(stage.connections.size()));
    for (const auto& connection : stage.connections) {
        detail::hash_scalar(hash, connection.first_city);
        detail::hash_scalar(hash, connection.second_city);
        detail::hash_scalar(hash, connection.terrain_cost);
        detail::hash_scalar(hash, static_cast<std::uint8_t>(connection.loop));
    }
    return hash;
}

}  // namespace aetheria::worldgen

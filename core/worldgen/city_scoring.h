#pragma once

// 聚落選址評分內部 helper，供 settlement_scoring.cpp 的上古／現代共用評分使用，
// 從原 city_sites.cpp 拆出。

#include "core/worldgen/region_generator.h"

#include <cstdint>
#include <span>
#include <vector>

namespace aetheria::worldgen::detail {

[[nodiscard]] std::vector<std::uint8_t>
ocean_connected_to_boundary(const QuantizedElevation& elevation);

[[nodiscard]] std::vector<std::uint8_t>
bottleneck_passability_mask(const QuantizedElevation& elevation, const BiomeStageOutput& biome,
                            const FeatureStageOutput& features, const rules::Ruleset& ruleset,
                            std::uint16_t barrier_move_cost);

[[nodiscard]] std::uint16_t local_bottleneck_score(std::span<const std::uint8_t> passable,
                                                   std::uint32_t width, std::uint32_t height,
                                                   std::size_t removed, std::uint8_t radius);

}  // namespace aetheria::worldgen::detail

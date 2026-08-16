#pragma once

// 城市選址評分內部 helper，供 city_sites.cpp 的 generate_cities 使用，
// 從 civilization_generator.cpp 拆出。

#include "core/worldgen/region_generator.h"

#include <cstdint>
#include <vector>

namespace aetheria::worldgen::detail {

[[nodiscard]] std::vector<std::uint8_t>
ocean_connected_to_boundary(const QuantizedElevation& elevation);

[[nodiscard]] std::uint16_t local_bottleneck_score(const QuantizedElevation& elevation,
                                                   std::size_t removed, std::uint8_t radius);

}  // namespace aetheria::worldgen::detail

#pragma once

// 文明生成共用 tile helper：跨 city_sites.cpp、road_path.cpp、road_network.cpp
// 共用的座標運算與基底 tiles 建置，從 civilization_generator.cpp 拆出。

#include "core/worldgen/region_generator.h"

#include "core/world/region_tiles.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace aetheria::worldgen::detail {

inline constexpr std::size_t kMissing = std::numeric_limits<std::size_t>::max();

[[nodiscard]] std::array<std::size_t, 4> neighbors(std::size_t index, std::uint32_t width,
                                                    std::uint32_t height) noexcept;

[[nodiscard]] world::RegionXY coordinate(std::size_t index, std::uint32_t width) noexcept;

void require_civilization_inputs(const QuantizedElevation& elevation,
                                 const ClimateStageOutput& climate, const RiverStageOutput& rivers,
                                 const BiomeStageOutput& biome,
                                 const FeatureStageOutput& features);

[[nodiscard]] world::RegionTiles
make_base_tiles(const QuantizedElevation& elevation, const ClimateStageOutput& climate,
                const RiverStageOutput& rivers, const BiomeStageOutput& biome,
                const FeatureStageOutput& features, const RegionDefinitionIds& definitions);

}  // namespace aetheria::worldgen::detail

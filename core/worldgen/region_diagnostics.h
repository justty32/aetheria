#pragma once

// region_diagnostics.h 收斂十階段雜湊、骨架／tile 雜湊、陸地連通檢查與灰階視覺化宣告。

#include "core/world/region_tiles.h"
#include "core/worldgen/region_civ_stages.h"
#include "core/worldgen/region_climate_stages.h"
#include "core/worldgen/region_relief_stages.h"
#include "core/worldgen/region_skeleton.h"

#include <cstdint>
#include <vector>

namespace aetheria::worldgen {

[[nodiscard]] std::uint64_t hash_stage(const PlateStageOutput& stage) noexcept;
[[nodiscard]] std::uint64_t hash_stage(const HeightStageOutput& stage) noexcept;
[[nodiscard]] std::uint64_t hash_stage(const ErosionStageOutput& stage) noexcept;
[[nodiscard]] std::uint64_t hash_stage(const ClimateStageOutput& stage) noexcept;
[[nodiscard]] std::uint64_t hash_stage(const RiverStageOutput& stage) noexcept;
[[nodiscard]] std::uint64_t hash_stage(const BiomeStageOutput& stage) noexcept;
[[nodiscard]] std::uint64_t hash_stage(const FeatureStageOutput& stage) noexcept;
[[nodiscard]] std::uint64_t hash_stage(const HistoryStageOutput& stage) noexcept;
[[nodiscard]] std::uint64_t hash_stage(const CityStageOutput& stage) noexcept;
[[nodiscard]] std::uint64_t hash_stage(const RoadStageOutput& stage) noexcept;
[[nodiscard]] std::uint64_t hash_skeleton(const RegionSkeleton& skeleton) noexcept;
[[nodiscard]] std::uint64_t hash_tiles(const world::RegionTiles& tiles) noexcept;
[[nodiscard]] double land_fraction(const RegionSkeleton& skeleton) noexcept;
[[nodiscard]] bool land_is_single_component(const RegionSkeleton& skeleton);

[[nodiscard]] std::vector<std::uint8_t> grayscale(const PlateStageOutput& stage);
[[nodiscard]] std::vector<std::uint8_t> grayscale(const HeightStageOutput& stage);
[[nodiscard]] std::vector<std::uint8_t> grayscale(const ErosionStageOutput& stage);
[[nodiscard]] std::vector<std::uint8_t> grayscale(const ClimateStageOutput& stage);
[[nodiscard]] std::vector<std::uint8_t> grayscale(const RiverStageOutput& stage);
[[nodiscard]] std::vector<std::uint8_t> grayscale(const BiomeStageOutput& stage);
[[nodiscard]] std::vector<std::uint8_t> grayscale(const FeatureStageOutput& stage);
[[nodiscard]] std::vector<std::uint8_t> grayscale(const HistoryStageOutput& stage);
[[nodiscard]] std::vector<std::uint8_t> grayscale(const CityStageOutput& stage);
[[nodiscard]] std::vector<std::uint8_t> grayscale(const RoadStageOutput& stage);

}  // namespace aetheria::worldgen

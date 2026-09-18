#include "core/rules/ruleset.h"
#include "core/world/region_tiles.h"
#include "core/worldgen/biome_classification.h"
#include "core/worldgen/gen_noise.h"
#include "core/worldgen/gen_stage_ids.h"
#include "core/worldgen/region_generator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <type_traits>

#include <gtest/gtest.h>

namespace {

using aetheria::rules::Ruleset;
using aetheria::worldgen::build_skeleton;
using aetheria::worldgen::ClimateStageOutput;
using aetheria::worldgen::derive_region_stage_seed;
using aetheria::worldgen::erode_height;
using aetheria::worldgen::ErosionGenerationConfig;
using aetheria::worldgen::ErosionStageOutput;
using aetheria::worldgen::generate_height;
using aetheria::worldgen::generate_plates;
using aetheria::worldgen::hash_stage;
using aetheria::worldgen::populate;
using aetheria::worldgen::ReliefClassificationInput;
using aetheria::worldgen::RegionBuildResult;
using aetheria::worldgen::RegionFastVariables;
using aetheria::worldgen::RegionGenerationConfig;
using aetheria::worldgen::RegionSkeleton;
using aetheria::worldgen::RegionSlowVariables;

struct CoastlineMetrics {
    std::size_t coastline_count{};
    std::size_t land_count{};
    std::size_t near_sea_level_count{};
    std::size_t plate_boundary_overlap_count{};
};

[[nodiscard]] CoastlineMetrics measure_coastline(const aetheria::worldgen::PlateStageOutput& plates,
                                                 const aetheria::worldgen::HeightStageOutput& height,
                                                 std::uint64_t height_seed,
                                                 const aetheria::worldgen::HeightGenerationConfig&
                                                     config) {
    CoastlineMetrics metrics;
    constexpr double kNearSeaLevelEpsilon = 100.0;
    for (std::uint32_t y = 0; y < height.height; ++y) {
        for (std::uint32_t x = 0; x < height.width; ++x) {
            const auto index = static_cast<std::size_t>(y) * height.width + x;
            if (height.land[index] != 0) {
                ++metrics.land_count;
                const auto touches_water =
                    (x > 0 && height.land[index - 1U] == 0) ||
                    (x + 1U < height.width && height.land[index + 1U] == 0) ||
                    (y > 0 && height.land[index - height.width] == 0) ||
                    (y + 1U < height.height && height.land[index + height.width] == 0);
                if (touches_water) {
                    ++metrics.coastline_count;
                    if (plates.boundary_type[index] !=
                        aetheria::worldgen::PlateBoundaryType::None) {
                        ++metrics.plate_boundary_overlap_count;
                    }
                }
            }

            const auto warped = aetheria::worldgen::detail::domain_warp(
                height_seed, x, y, config.coast_warp_wavelength, config.coast_warp_amplitude);
            const auto sample_x = std::clamp<std::int64_t>(warped.x, 0, plates.width - 1U);
            const auto sample_y = std::clamp<std::int64_t>(warped.y, 0, plates.height - 1U);
            const auto sample_index = static_cast<std::size_t>(sample_y) * plates.width +
                                      static_cast<std::size_t>(sample_x);
            const auto plate_index = plates.plate_index[sample_index];
            const auto raw_elevation = plates.plates[plate_index].base_elevation +
                                       plates.boundary_effect[index] +
                                       aetheria::worldgen::detail::fbm(
                                           height_seed, x, y,
                                           config.noise_octaves);
            if (std::abs(raw_elevation - height.sea_level) <= kNearSeaLevelEpsilon) {
                ++metrics.near_sea_level_count;
            }
        }
    }
    return metrics;
}

TEST(RegionGenerationStage, CoastlineQualityMetricsRemainVisible) {
    constexpr std::array seeds{UINT64_C(515151), UINT64_C(12345), UINT64_C(424242)};
    constexpr std::array baseline_fractal_ratios{0.125068, 0.033912, 0.061856};
    constexpr std::array baseline_overlap_ratios{0.470716, 0.248000, 0.552632};
    const RegionSlowVariables slow{0, 128, 96};
    const aetheria::worldgen::HeightGenerationConfig height_config;
    std::size_t total_coastline_count{};
    std::size_t total_land_count{};
    std::size_t total_overlap_count{};
    for (std::size_t seed_index = 0; seed_index < seeds.size(); ++seed_index) {
        const auto seed = seeds[seed_index];
        const auto plate_seed = derive_region_stage_seed(
            seed, slow.region_id, aetheria::worldgen::detail::kPlateStageId);
        const auto height_seed = derive_region_stage_seed(
            seed, slow.region_id, aetheria::worldgen::detail::kHeightStageId);
        const auto plates = generate_plates(slow, plate_seed, {});
        const auto height = generate_height(plates, height_seed, height_config);
        const auto metrics = measure_coastline(plates, height, height_seed, height_config);
        const auto fractal_ratio = static_cast<double>(metrics.coastline_count) /
                                   static_cast<double>(metrics.land_count);
        const auto overlap_ratio = static_cast<double>(metrics.plate_boundary_overlap_count) /
                                   static_cast<double>(metrics.coastline_count);

        std::cout << std::fixed << std::setprecision(6) << "coastline seed=" << seed
                  << " coast=" << metrics.coastline_count << " land=" << metrics.land_count
                  << " fractal_ratio=" << fractal_ratio
                  << " near_sea_100m=" << metrics.near_sea_level_count
                  << " plate_overlap=" << metrics.plate_boundary_overlap_count
                  << " plate_overlap_ratio=" << overlap_ratio << '\n';
        EXPECT_GT(metrics.coastline_count, 0U);
        EXPECT_GT(metrics.near_sea_level_count, 0U);
        EXPECT_EQ(metrics.land_count, 3686U);
        EXPECT_GT(fractal_ratio, baseline_fractal_ratios[seed_index]);
        EXPECT_LT(overlap_ratio, baseline_overlap_ratios[seed_index]);
        total_coastline_count += metrics.coastline_count;
        total_land_count += metrics.land_count;
        total_overlap_count += metrics.plate_boundary_overlap_count;
    }
    constexpr std::size_t kBaselineCoastlineCount = 814;
    constexpr std::size_t kBaselineOverlapCount = 374;
    constexpr std::size_t kBaselineLandCount = 3686U * seeds.size();
    EXPECT_GT(total_coastline_count * 100U, kBaselineCoastlineCount * 125U);
    EXPECT_LT(total_overlap_count * kBaselineCoastlineCount * 10U,
              kBaselineOverlapCount * total_coastline_count * 6U);
    EXPECT_EQ(total_land_count, kBaselineLandCount);
}

}  // namespace

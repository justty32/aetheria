#include "sim/terrain_metrics.h"

#include "core/worldgen/gen_grid.h"
#include "core/worldgen/region_generator.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

namespace aetheria::sim {
namespace {

constexpr std::size_t kHistogramBins = 16;

struct LandHistogram {
  std::uint16_t minimum{};
  std::uint16_t median{};
  std::uint16_t percentile_95{};
  std::uint16_t maximum{};
  std::uint32_t bin_width{1};
  std::array<std::size_t, kHistogramBins> counts{};
};

template <typename Values>
[[nodiscard]] LandHistogram
histogram_on_land(const Values &values, const std::vector<std::uint8_t> &land) {
  std::vector<std::uint16_t> samples;
  samples.reserve(land.size());
  for (std::size_t index = 0; index < land.size(); ++index) {
    if (land[index] != 0) {
      samples.push_back(values[index]);
    }
  }
  LandHistogram histogram;
  if (samples.empty()) {
    return histogram;
  }
  std::ranges::sort(samples);
  histogram.minimum = samples.front();
  histogram.median = samples[samples.size() / 2U];
  histogram.percentile_95 = samples[(samples.size() - 1U) * 95U / 100U];
  histogram.maximum = samples.back();
  const auto range =
      static_cast<std::uint32_t>(histogram.maximum) - histogram.minimum + 1U;
  histogram.bin_width = (range + kHistogramBins - 1U) / kHistogramBins;
  for (const auto value : samples) {
    const auto bin = std::min<std::size_t>(
        (static_cast<std::uint32_t>(value) - histogram.minimum) /
            histogram.bin_width,
        kHistogramBins - 1U);
    ++histogram.counts[bin];
  }
  return histogram;
}

void print_histogram(const char *name, const LandHistogram &histogram) {
  std::cout << name << " min=" << histogram.minimum
            << " median=" << histogram.median
            << " p95=" << histogram.percentile_95
            << " max=" << histogram.maximum
            << " bin_width=" << histogram.bin_width << " counts=";
  for (std::size_t index = 0; index < histogram.counts.size(); ++index) {
    std::cout << (index == 0 ? "" : ",") << histogram.counts[index];
  }
  std::cout << '\n';
}

} // namespace

int run_terrain_metrics(const aetheria::rules::Ruleset &ruleset,
                        std::uint64_t seed, std::uint32_t region_id,
                        std::int16_t latitude_degrees) {
  const auto result = aetheria::worldgen::build_skeleton(
      aetheria::worldgen::RegionSlowVariables{region_id, 128, 96,
                                              latitude_degrees},
      seed, ruleset);
  const auto &elevation = result.skeleton.elevation;
  const auto count = aetheria::worldgen::detail::checked_count(
      elevation.width, elevation.height);
  std::vector<std::size_t> terrain_counts(ruleset.terrains().size());
  std::size_t land_count{};
  std::size_t coastline_count{};
  std::size_t plate_overlap_count{};
  std::size_t saturated_moisture_count{};
  for (std::size_t index = 0; index < count; ++index) {
    if (elevation.land[index] == 0) {
      continue;
    }
    ++land_count;
    ++terrain_counts.at(aetheria::rules::value_of(result.biome.terrain[index]));
    saturated_moisture_count += result.rivers.moisture[index] == UINT16_MAX;
    const auto adjacent = aetheria::worldgen::detail::neighbors(
        index, elevation.width, elevation.height);
    bool touches_water{};
    for (const auto neighbor : adjacent) {
      touches_water =
          touches_water || (neighbor < count && elevation.land[neighbor] == 0);
    }
    if (!touches_water) {
      continue;
    }
    ++coastline_count;
    plate_overlap_count += result.plates.boundary_type[index] !=
                           aetheria::worldgen::PlateBoundaryType::None;
  }

  const auto ratio = [](std::size_t numerator, std::size_t denominator) {
    return denominator == 0 ? 0.0
                            : static_cast<double>(numerator) / denominator;
  };
  std::cout << std::fixed << std::setprecision(6)
            << "terrain_metrics seed=" << seed << " region=" << region_id
            << " latitude=" << latitude_degrees << " land=" << land_count
            << " coastline=" << coastline_count
            << " fractal_ratio=" << ratio(coastline_count, land_count)
            << " plate_overlap=" << plate_overlap_count
            << " plate_overlap_ratio="
            << ratio(plate_overlap_count, coastline_count)
            << " moisture_saturated=" << saturated_moisture_count
            << " moisture_saturated_ratio="
            << ratio(saturated_moisture_count, land_count) << '\n';
  print_histogram("elevation_histogram",
                  histogram_on_land(elevation.meters, elevation.land));
  print_histogram("moisture_histogram",
                  histogram_on_land(result.rivers.moisture, elevation.land));
  std::cout << "terrain_histogram";
  for (std::size_t index = 0; index < terrain_counts.size(); ++index) {
    const auto *terrain =
        ruleset.terrain(static_cast<aetheria::rules::TerrainId>(index));
    if (terrain != nullptr && terrain_counts[index] != 0) {
      std::cout << ' ' << terrain->id << '=' << terrain_counts[index];
    }
  }
  std::cout << '\n';
  return 0;
}

} // namespace aetheria::sim

#include "core/worldgen/region_diagnostics.h"

#include "core/worldgen/gen_grid.h"
#include "core/worldgen/gen_hash.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace aetheria::worldgen {

double land_fraction(const RegionSkeleton& skeleton) noexcept {
    if (skeleton.elevation.land.empty()) {
        return 0.0;
    }
    const auto count = std::count_if(skeleton.elevation.land.begin(), skeleton.elevation.land.end(),
                                     [](std::uint8_t value) { return value != 0; });
    return static_cast<double>(count) / static_cast<double>(skeleton.elevation.land.size());
}

bool land_is_single_component(const RegionSkeleton& skeleton) {
    const auto count = detail::checked_count(skeleton.elevation.width, skeleton.elevation.height);
    if (skeleton.elevation.land.size() != count) {
        return false;
    }
    const auto total_land = static_cast<std::size_t>(
        std::count_if(skeleton.elevation.land.begin(), skeleton.elevation.land.end(),
                      [](std::uint8_t value) { return value != 0; }));
    if (total_land == 0) {
        return false;
    }
    return detail::largest_land_component(skeleton.elevation.land, skeleton.elevation.width,
                                  skeleton.elevation.height)
               .size() == total_land;
}

std::vector<std::uint8_t> grayscale(const PlateStageOutput& stage) {
    if (stage.plates.empty()) {
        return {};
    }
    std::vector<std::uint8_t> pixels;
    pixels.reserve(stage.plate_index.size());
    for (const auto plate : stage.plate_index) {
        pixels.push_back(
            static_cast<std::uint8_t>((static_cast<unsigned>(plate) * 255U) /
                                      std::max<std::size_t>(1, stage.plates.size() - 1U)));
    }
    return pixels;
}

std::vector<std::uint8_t> grayscale(const HeightStageOutput& stage) {
    return detail::grayscale_values(stage.elevation);
}

std::vector<std::uint8_t> grayscale(const ErosionStageOutput& stage) {
    return detail::grayscale_values(stage.elevation);
}

std::vector<std::uint8_t> grayscale(const ClimateStageOutput& stage) {
    std::vector<std::uint8_t> pixels;
    pixels.reserve(stage.moisture.size());
    for (const auto moisture : stage.moisture) {
        pixels.push_back(static_cast<std::uint8_t>(moisture / 257U));
    }
    return pixels;
}

std::vector<std::uint8_t> grayscale(const RiverStageOutput& stage) {
    std::vector<std::uint8_t> pixels;
    pixels.reserve(stage.river_class.size());
    for (const auto river_class : stage.river_class) {
        pixels.push_back(static_cast<std::uint8_t>(river_class * 85U));
    }
    return pixels;
}

std::vector<std::uint8_t> grayscale(const BiomeStageOutput& stage) {
    std::vector<std::uint8_t> pixels;
    pixels.reserve(stage.terrain.size());
    for (std::size_t index = 0; index < stage.terrain.size(); ++index) {
        pixels.push_back(static_cast<std::uint8_t>((rules::value_of(stage.terrain[index]) * 53U +
                                                    rules::value_of(stage.relief[index]) * 17U) &
                                                   UINT8_MAX));
    }
    return pixels;
}

std::vector<std::uint8_t> grayscale(const FeatureStageOutput& stage) {
    std::vector<std::uint8_t> pixels;
    pixels.reserve(stage.feature.size());
    for (const auto feature : stage.feature) {
        pixels.push_back(static_cast<std::uint8_t>((rules::value_of(feature) * 61U) & UINT8_MAX));
    }
    return pixels;
}

std::vector<std::uint8_t> grayscale(const CityStageOutput& stage) {
    std::vector<std::uint8_t> pixels(stage.score.size());
    for (const auto& city : stage.cities) {
        if (city.canonical_id < pixels.size()) {
            pixels[city.canonical_id] = static_cast<std::uint8_t>(city.tier) * 85U;
        }
    }
    return pixels;
}

std::vector<std::uint8_t> grayscale(const RoadStageOutput& stage) {
    const auto count = static_cast<std::size_t>(stage.width) * stage.height;
    std::vector<std::uint8_t> pixels(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto begin = index * 4U;
        const auto maximum = *std::max_element(stage.usage.begin() + static_cast<std::ptrdiff_t>(begin),
                                               stage.usage.begin() + static_cast<std::ptrdiff_t>(begin + 4U));
        pixels[index] = static_cast<std::uint8_t>(std::min<unsigned>(255U, maximum * 64U));
    }
    return pixels;
}

}  // namespace aetheria::worldgen

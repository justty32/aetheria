#include "core/worldgen/region_relief_stages.h"

#include "core/worldgen/gen_grid.h"
#include "core/worldgen/gen_stage_ids.h"
#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace aetheria::worldgen {
namespace {

[[nodiscard]] std::uint16_t quantize_meter(double value) {
    if (!std::isfinite(value)) {
        throw std::runtime_error{"高度場含非有限浮點值"};
    }
    const auto clamped = std::clamp(value, detail::kMinWorldElevation, detail::kMaxWorldElevation);
    return static_cast<std::uint16_t>(std::llround(clamped) -
                                      static_cast<long long>(detail::kMinWorldElevation));
}

}  // namespace

ErosionStageOutput erode_height(const HeightStageOutput& height, std::uint64_t stage_seed,
                                const ErosionGenerationConfig& config) {
    const auto count = detail::checked_count(height.width, height.height);
    if (height.elevation.size() != count || height.land.size() != count) {
        throw std::invalid_argument{"高度場階段輸出尺寸不一致"};
    }
    if (!std::isfinite(config.talus) || config.talus < 0.0 ||
        !std::isfinite(config.transfer_fraction) || config.transfer_fraction <= 0.0 ||
        config.transfer_fraction > 0.5) {
        throw std::invalid_argument{"侵蝕參數超出範圍"};
    }

    ErosionStageOutput output{height.width, height.height, height.elevation, height.land,
                              height.sea_level};
    std::vector<double> delta(count);
    for (std::uint16_t iteration = 0; iteration < config.iterations; ++iteration) {
        std::fill(delta.begin(), delta.end(), 0.0);
        for (std::size_t index = 0; index < count; ++index) {
            const auto adjacent = detail::neighbors(index, height.width, height.height);
            const auto rotation = static_cast<std::size_t>(
                splitmix64(stage_seed ^ index ^ iteration) % adjacent.size());
            auto lowest = index;
            for (std::size_t offset = 0; offset < adjacent.size(); ++offset) {
                const auto neighbor = adjacent[(rotation + offset) % adjacent.size()];
                if (neighbor < count && output.elevation[neighbor] < output.elevation[lowest]) {
                    lowest = neighbor;
                }
            }
            const auto difference = output.elevation[index] - output.elevation[lowest];
            if (lowest != index && difference > config.talus) {
                const auto transfer = (difference - config.talus) * config.transfer_fraction;
                delta[index] -= transfer;
                delta[lowest] += transfer;
            }
        }
        for (std::size_t index = 0; index < count; ++index) {
            output.elevation[index] += delta[index];
        }
    }
    return output;
}

QuantizedElevation quantize_elevation(const ErosionStageOutput& erosion) {
    const auto count = detail::checked_count(erosion.width, erosion.height);
    if (erosion.elevation.size() != count || erosion.land.size() != count ||
        !std::isfinite(erosion.sea_level)) {
        throw std::invalid_argument{"侵蝕階段輸出尺寸或海平面無效"};
    }

    QuantizedElevation output{erosion.width, erosion.height, {}, {}, 0};
    output.sea_level = quantize_meter(erosion.sea_level);
    output.land = erosion.land;
    output.meters.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto adjusted = erosion.land[index] != 0
                                  ? std::max(erosion.elevation[index], erosion.sea_level + 1.0)
                                  : std::min(erosion.elevation[index], erosion.sea_level - 1.0);
        output.meters.push_back(quantize_meter(adjusted));
    }
    return output;
}

}  // namespace aetheria::worldgen

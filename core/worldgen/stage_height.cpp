#include "core/worldgen/region_relief_stages.h"

#include "core/worldgen/gen_grid.h"
#include "core/worldgen/gen_noise.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace aetheria::worldgen {
namespace {

[[nodiscard]] std::size_t warped_plate_index(const PlateStageOutput& plates,
                                             std::uint64_t stage_seed, std::uint32_t x,
                                             std::uint32_t y,
                                             const HeightGenerationConfig& config) noexcept {
    const auto warped = detail::domain_warp(stage_seed, x, y, config.coast_warp_wavelength,
                                            config.coast_warp_amplitude);
    const auto sample_x = std::clamp<std::int64_t>(warped.x, 0, plates.width - 1U);
    const auto sample_y = std::clamp<std::int64_t>(warped.y, 0, plates.height - 1U);
    return static_cast<std::size_t>(sample_y) * plates.width +
           static_cast<std::size_t>(sample_x);
}

[[nodiscard]] std::vector<std::uint8_t>
repair_land_connectivity(const std::vector<double>& elevation,
                         const std::vector<std::uint8_t>& initial_land, std::uint32_t width,
                         std::uint32_t height, std::size_t target_count) {
    auto seed_component = detail::largest_land_component(initial_land, width, height);
    if (seed_component.empty()) {
        seed_component.push_back(static_cast<std::size_t>(std::distance(
            elevation.begin(), std::max_element(elevation.begin(), elevation.end()))));
    }

    const auto best_seed = *std::max_element(
        seed_component.begin(), seed_component.end(),
        [&](std::size_t lhs, std::size_t rhs) { return elevation[lhs] < elevation[rhs]; });
    if (seed_component.size() > target_count) {
        seed_component.assign(1, best_seed);
    }

    std::vector<std::uint8_t> selected(elevation.size());
    std::vector<std::uint8_t> queued(elevation.size());
    using Candidate = std::pair<double, std::size_t>;
    std::priority_queue<Candidate> frontier;
    for (const auto index : seed_component) {
        selected[index] = 1;
    }
    auto queue_neighbors = [&](std::size_t index) {
        for (const auto neighbor : detail::neighbors(index, width, height)) {
            if (neighbor < elevation.size() && selected[neighbor] == 0 && queued[neighbor] == 0) {
                queued[neighbor] = 1;
                frontier.emplace(elevation[neighbor], neighbor);
            }
        }
    };
    for (const auto index : seed_component) {
        queue_neighbors(index);
    }

    auto selected_count = seed_component.size();
    while (selected_count < target_count && !frontier.empty()) {
        const auto index = frontier.top().second;
        frontier.pop();
        if (selected[index] != 0) {
            continue;
        }
        selected[index] = 1;
        ++selected_count;
        queue_neighbors(index);
    }
    return selected;
}

}  // namespace

HeightStageOutput generate_height(const PlateStageOutput& plates, std::uint64_t stage_seed,
                                  const HeightGenerationConfig& config) {
    const auto count = detail::checked_count(plates.width, plates.height);
    if (plates.plate_index.size() != count || plates.boundary_type.size() != count ||
        plates.boundary_effect.size() != count || plates.plates.empty()) {
        throw std::invalid_argument{"板塊階段輸出尺寸不一致"};
    }
    if (config.noise_octaves == 0 || config.noise_octaves > 8 || config.target_land_percent == 0 ||
        config.target_land_percent >= 100 || config.coast_warp_wavelength == 0 ||
        !std::isfinite(config.coast_warp_amplitude) || config.coast_warp_amplitude < 0.0) {
        throw std::invalid_argument{"高度場參數超出範圍"};
    }

    HeightStageOutput output{plates.width, plates.height, {}, {}, 0.0};
    output.elevation.resize(count);
    for (std::uint32_t y = 0; y < plates.height; ++y) {
        for (std::uint32_t x = 0; x < plates.width; ++x) {
            const auto index = static_cast<std::size_t>(y) * plates.width + x;
            const auto plate_index = plates.plate_index[index];
            if (plate_index >= plates.plates.size()) {
                throw std::invalid_argument{"板塊階段輸出含無效 plate index"};
            }
            const auto base_plate_index = plates.plate_index[warped_plate_index(
                plates, stage_seed, x, y, config)];
            if (base_plate_index >= plates.plates.size()) {
                throw std::invalid_argument{"板塊階段輸出含無效 plate index"};
            }
            output.elevation[index] = plates.plates[base_plate_index].base_elevation +
                                      plates.boundary_effect[index] +
                                      detail::fbm(stage_seed, x, y, config.noise_octaves);
        }
    }

    const auto [minimum, maximum] =
        std::minmax_element(output.elevation.begin(), output.elevation.end());
    auto low = *minimum - 1.0;
    auto high = *maximum + 1.0;
    const auto target_count = std::max<std::size_t>(
        1, count * static_cast<std::size_t>(config.target_land_percent) / 100U);
    for (std::uint8_t iteration = 0; iteration < 48; ++iteration) {
        const auto middle = (low + high) * 0.5;
        const auto land_count =
            static_cast<std::size_t>(std::count_if(output.elevation.begin(), output.elevation.end(),
                                                   [&](double value) { return value >= middle; }));
        if (land_count > target_count) {
            low = middle;
        } else {
            high = middle;
        }
    }
    output.sea_level = (low + high) * 0.5;
    std::vector<std::uint8_t> initial_land(count);
    for (std::size_t index = 0; index < count; ++index) {
        initial_land[index] = output.elevation[index] >= output.sea_level ? UINT8_C(1) : UINT8_C(0);
    }
    output.land = repair_land_connectivity(output.elevation, initial_land, plates.width,
                                           plates.height, target_count);
    for (std::size_t index = 0; index < count; ++index) {
        if (output.land[index] != 0) {
            output.elevation[index] = std::max(output.elevation[index], output.sea_level + 1.0);
        } else {
            output.elevation[index] = std::min(output.elevation[index], output.sea_level - 1.0);
        }
    }
    return output;
}

}  // namespace aetheria::worldgen

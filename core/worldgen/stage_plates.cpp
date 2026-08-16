#include "core/worldgen/region_relief_stages.h"

#include "core/worldgen/gen_grid.h"
#include "core/worldgen/gen_noise.h"

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <queue>
#include <stdexcept>

namespace aetheria::worldgen {
namespace {

struct BoundaryInteraction {
    PlateBoundaryType type;
    std::int16_t effect;
};

[[nodiscard]] BoundaryInteraction boundary_interaction(const Plate& lhs,
                                                       const Plate& rhs) noexcept {
    const auto separation_x = static_cast<std::int32_t>(rhs.x) - lhs.x;
    const auto separation_y = static_cast<std::int32_t>(rhs.y) - lhs.y;
    const auto relative_x = static_cast<std::int32_t>(rhs.drift_x) - lhs.drift_x;
    const auto relative_y = static_cast<std::int32_t>(rhs.drift_y) - lhs.drift_y;
    const auto dot = relative_x * separation_x + relative_y * separation_y;
    if (dot < -8) {
        return {PlateBoundaryType::Convergent, 1200};
    }
    if (dot > 8) {
        return {PlateBoundaryType::Divergent,
                static_cast<std::int16_t>(lhs.is_oceanic && rhs.is_oceanic ? 320 : -650)};
    }
    return {PlateBoundaryType::Transform, 0};
}

void write_stronger(PlateBoundaryType& target_type, std::int16_t& target_effect,
                    BoundaryInteraction candidate) noexcept {
    if (target_type == PlateBoundaryType::None ||
        std::abs(static_cast<int>(candidate.effect)) > std::abs(static_cast<int>(target_effect))) {
        target_type = candidate.type;
        target_effect = candidate.effect;
    }
}

}  // namespace

PlateStageOutput generate_plates(const RegionSlowVariables& slow, std::uint64_t stage_seed,
                                 const PlateGenerationConfig& config) {
    const auto count = detail::checked_count(slow.width, slow.height);
    if (config.min_count < 8 || config.max_count > 16 || config.min_count > config.max_count) {
        throw std::invalid_argument{"板塊數範圍必須落在 8..16"};
    }

    PlateStageOutput output{slow.width, slow.height, {}, {}, {}, {}};
    detail::SplitMix64Stream random{stage_seed};
    const auto plate_count = static_cast<std::size_t>(
        config.min_count + random.bounded(config.max_count - config.min_count + 1U));
    output.plates.reserve(plate_count);
    for (std::size_t index = 0; index < plate_count; ++index) {
        const auto oceanic = random.bounded(100) < 48;
        auto drift_x = static_cast<std::int8_t>(static_cast<int>(random.bounded(9)) - 4);
        auto drift_y = static_cast<std::int8_t>(static_cast<int>(random.bounded(9)) - 4);
        if (drift_x == 0 && drift_y == 0) {
            drift_x = 1;
        }
        const auto base = oceanic ? -1500 + static_cast<int>(random.bounded(900))
                                  : 250 + static_cast<int>(random.bounded(1350));
        output.plates.push_back({static_cast<std::uint16_t>(random.bounded(slow.width)),
                                 static_cast<std::uint16_t>(random.bounded(slow.height)), oceanic,
                                 drift_x, drift_y, static_cast<std::int16_t>(base)});
    }

    output.plate_index.resize(count);
    for (std::uint32_t y = 0; y < slow.height; ++y) {
        for (std::uint32_t x = 0; x < slow.width; ++x) {
            std::uint64_t closest_distance = std::numeric_limits<std::uint64_t>::max();
            std::uint8_t closest{};
            for (std::size_t plate_index = 0; plate_index < output.plates.size(); ++plate_index) {
                const auto dx = static_cast<std::int64_t>(x) - output.plates[plate_index].x;
                const auto dy = static_cast<std::int64_t>(y) - output.plates[plate_index].y;
                const auto distance = static_cast<std::uint64_t>(dx * dx + dy * dy);
                if (distance < closest_distance) {
                    closest_distance = distance;
                    closest = static_cast<std::uint8_t>(plate_index);
                }
            }
            output.plate_index[static_cast<std::size_t>(y) * slow.width + x] = closest;
        }
    }

    output.boundary_type.assign(count, PlateBoundaryType::None);
    output.boundary_effect.assign(count, 0);
    for (std::size_t index = 0; index < count; ++index) {
        for (const auto neighbor : detail::neighbors(index, slow.width, slow.height)) {
            if (neighbor >= count || output.plate_index[index] == output.plate_index[neighbor]) {
                continue;
            }
            const auto effect = boundary_interaction(output.plates[output.plate_index[index]],
                                                     output.plates[output.plate_index[neighbor]]);
            write_stronger(output.boundary_type[index], output.boundary_effect[index], effect);
            write_stronger(output.boundary_type[neighbor], output.boundary_effect[neighbor],
                           effect);
        }
    }

    std::queue<std::size_t> frontier;
    std::vector<std::uint8_t> distance(count, UINT8_MAX);
    for (std::size_t index = 0; index < count; ++index) {
        if (output.boundary_effect[index] != 0) {
            distance[index] = 0;
            frontier.push(index);
        }
    }
    while (!frontier.empty()) {
        const auto current = frontier.front();
        frontier.pop();
        if (distance[current] >= 8) {
            continue;
        }
        for (const auto neighbor : detail::neighbors(current, slow.width, slow.height)) {
            if (neighbor >= count || distance[neighbor] != UINT8_MAX) {
                continue;
            }
            distance[neighbor] = static_cast<std::uint8_t>(distance[current] + 1U);
            output.boundary_effect[neighbor] =
                static_cast<std::int16_t>((output.boundary_effect[current] * 3) / 4);
            frontier.push(neighbor);
        }
    }
    return output;
}

}  // namespace aetheria::worldgen

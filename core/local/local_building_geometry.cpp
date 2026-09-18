#include <algorithm>
#include <cstdint>
#include <vector>

#include "core/local/local_building_detail.h"
#include "core/worldgen/region_seed.h"

namespace aetheria::local::detail {
namespace {

constexpr std::uint64_t kHouseSalt = UINT64_C(0x6D43F21AB895C70E);
constexpr std::uint16_t kHouseGap = 1;

struct Segment {
    std::uint16_t start{};
    std::uint16_t extent{};
};

[[nodiscard]] std::vector<Segment> split_frontage(std::uint16_t start, std::uint16_t extent,
                                                  std::uint8_t minimum, std::uint8_t maximum,
                                                  std::uint64_t seed) {
    const auto minimum_count = static_cast<std::uint16_t>(
        (extent + kHouseGap + maximum + kHouseGap - 1U) / (maximum + kHouseGap));
    const auto maximum_count =
        static_cast<std::uint16_t>((extent + kHouseGap) / (minimum + kHouseGap));
    const auto count = static_cast<std::uint16_t>(
        minimum_count + worldgen::splitmix64(seed) % (maximum_count - minimum_count + 1U));
    std::vector<Segment> result;
    result.reserve(count);
    auto cursor = start;
    auto remaining =
        static_cast<std::uint16_t>(extent - (count - 1U) * kHouseGap);
    for (std::uint16_t index = 0; index < count; ++index) {
        const auto segments_after = static_cast<std::uint16_t>(count - index - 1U);
        const auto maximum_after = static_cast<std::uint16_t>(segments_after * maximum);
        const auto smallest =
            remaining > maximum_after
                ? std::max<std::uint16_t>(minimum,
                                          static_cast<std::uint16_t>(remaining - maximum_after))
                : minimum;
        const auto largest = std::min<std::uint16_t>(
            maximum, static_cast<std::uint16_t>(remaining - segments_after * minimum));
        const auto sample = worldgen::splitmix64(seed ^ index);
        const auto width =
            static_cast<std::uint16_t>(smallest + sample % (largest - smallest + 1U));
        result.push_back({cursor, width});
        cursor = static_cast<std::uint16_t>(cursor + width + kHouseGap);
        remaining = static_cast<std::uint16_t>(remaining - width);
    }
    return result;
}

void add_row(std::vector<LocalHouse>& houses, spatial::BoundarySide frontage, std::uint16_t start,
             std::uint16_t extent, std::uint16_t fixed, const rules::LocalBuildingRules& config,
             std::uint64_t seed) {
    for (const auto segment : split_frontage(start, extent, config.house_frontage_min,
                                             config.house_frontage_max, seed)) {
        spatial::PartitionRect footprint;
        if (frontage == spatial::BoundarySide::North || frontage == spatial::BoundarySide::South) {
            footprint = {segment.start, fixed, segment.extent, config.house_depth};
        } else {
            footprint = {fixed, segment.start, config.house_depth, segment.extent};
        }
        houses.push_back({footprint, frontage});
    }
}

void add_vertical_link(BuildingLocalSkeleton& result, std::uint16_t house_index,
                       std::int8_t destination) {
    const auto& rect = result.houses[house_index].footprint;
    const auto x = destination > 0 ? static_cast<std::uint16_t>(rect.x + 1U)
                                   : static_cast<std::uint16_t>(rect.x + rect.width - 2U);
    const auto y = static_cast<std::uint16_t>(rect.y + 1U);
    const LocalXY tile{x, y};
    result.layers.at(0).overlay[tile_index(x, y)] = OverlayId::Stairs;
    result.layers.at(destination).overlay[tile_index(x, y)] = OverlayId::Stairs;
    result.vertical_links.push_back({tile, 0, destination});
}

}  // namespace

void build_house_geometry(BuildingLocalSkeleton& result, const LocalSlowVars& slow,
                          std::uint64_t local_seed, const rules::Ruleset& ruleset) {
    const auto& config = ruleset.local_building_rules();
    const auto margin = static_cast<std::uint16_t>(config.house_margin);
    const auto depth = static_cast<std::uint16_t>(config.house_depth);
    const auto outer_extent = static_cast<std::uint16_t>(kLocalWidth - margin * 2U);
    const auto middle_start = static_cast<std::uint16_t>(margin + depth);
    const auto side_start = static_cast<std::uint16_t>(middle_start + 1U);
    const auto side_extent = static_cast<std::uint16_t>(kLocalHeight - 2U * side_start);
    add_row(result.houses, spatial::BoundarySide::North, margin, outer_extent, margin, config,
            local_seed ^ kHouseSalt);
    add_row(result.houses, spatial::BoundarySide::South, margin, outer_extent,
            static_cast<std::uint16_t>(kLocalHeight - margin - depth), config,
            local_seed ^ kHouseSalt ^ UINT64_C(0x10));
    add_row(result.houses, spatial::BoundarySide::West, side_start, side_extent, margin,
            config,
            local_seed ^ kHouseSalt ^ UINT64_C(0x20));
    add_row(result.houses, spatial::BoundarySide::East, side_start, side_extent,
            static_cast<std::uint16_t>(kLocalWidth - margin - depth), config,
            local_seed ^ kHouseSalt ^ UINT64_C(0x30));
    result.houses.push_back(
        {{static_cast<std::uint16_t>(middle_start + 1U),
          static_cast<std::uint16_t>(middle_start + 1U), config.house_frontage_min, depth},
         spatial::BoundarySide::East});

    const auto no_edge = *ruleset.find_edge("edge.none");
    for (std::size_t index = 0; index < result.houses.size(); ++index) {
        auto& house = result.houses[index];
        const auto seed = worldgen::splitmix64(local_seed ^ kHouseSalt ^ index);
        const auto resident_span = config.residents_max - config.residents_min + 1U;
        house.resident_count =
            static_cast<std::uint16_t>(config.residents_min + seed % resident_span);
        house.has_upper_floor = index == 0 || (seed >> 8U) % 100U < config.upper_floor_percent;
        house.has_cellar = index == 0 || (seed >> 16U) % 100U < config.cellar_percent;
        result.resident_statistics += house.resident_count;
        if (house.has_upper_floor && !result.layers.contains(1)) {
            result.layers.emplace(1, make_local_layer(config.foundation_ground, no_edge, 192));
        }
        if (house.has_cellar && !result.layers.contains(-1)) {
            result.layers.emplace(-1, make_local_layer(config.foundation_ground, no_edge, 96));
        }
        build_floor(result, slow, static_cast<std::uint16_t>(index), 0, seed, ruleset);
        if (house.has_upper_floor) {
            build_floor(result, slow, static_cast<std::uint16_t>(index), 1, seed ^ UINT64_C(0x100),
                        ruleset);
            add_vertical_link(result, static_cast<std::uint16_t>(index), 1);
        }
        if (house.has_cellar) {
            build_floor(result, slow, static_cast<std::uint16_t>(index), -1, seed ^ UINT64_C(0x200),
                        ruleset);
            add_vertical_link(result, static_cast<std::uint16_t>(index), -1);
        }
    }
}

}  // namespace aetheria::local::detail

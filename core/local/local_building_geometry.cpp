#include <algorithm>
#include <cstdint>
#include <vector>

#include "core/local/local_building_detail.h"
#include "core/worldgen/region_seed.h"

namespace aetheria::local::detail {
namespace {

constexpr std::uint64_t kHouseSalt = UINT64_C(0x6D43F21AB895C70E);
constexpr std::uint64_t kRoomSalt = UINT64_C(0x947C02D5A613EB8F);

struct Segment {
    std::uint16_t start{};
    std::uint16_t extent{};
};

[[nodiscard]] std::vector<Segment> split_frontage(std::uint16_t start, std::uint16_t extent,
                                                  std::uint8_t minimum, std::uint8_t maximum,
                                                  std::uint64_t seed) {
    const auto minimum_count = static_cast<std::uint16_t>((extent + maximum - 1U) / maximum);
    const auto maximum_count = static_cast<std::uint16_t>(extent / minimum);
    const auto count = static_cast<std::uint16_t>(
        minimum_count + worldgen::splitmix64(seed) % (maximum_count - minimum_count + 1U));
    std::vector<Segment> result;
    result.reserve(count);
    auto cursor = start;
    auto remaining = extent;
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
        cursor = static_cast<std::uint16_t>(cursor + width);
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

void paint_outer_walls(LocalTiles& tiles, spatial::PartitionRect rect, rules::EdgeId wall) {
    for (std::uint16_t x = rect.x; x < rect.x + rect.width; ++x) {
        set_edge(tiles, {x, rect.y}, spatial::BoundarySide::North, wall);
        set_edge(tiles, {x, static_cast<std::uint16_t>(rect.y + rect.height - 1U)},
                 spatial::BoundarySide::South, wall);
    }
    for (std::uint16_t y = rect.y; y < rect.y + rect.height; ++y) {
        set_edge(tiles, {rect.x, y}, spatial::BoundarySide::West, wall);
        set_edge(tiles, {static_cast<std::uint16_t>(rect.x + rect.width - 1U), y},
                 spatial::BoundarySide::East, wall);
    }
}

void paint_cut(LocalTiles& tiles, const spatial::PartitionCut& cut, rules::EdgeId wall,
               rules::EdgeId door, std::uint16_t& door_count) {
    for (std::uint16_t offset = 0; offset < cut.extent; ++offset) {
        if (cut.vertical) {
            set_edge(tiles,
                     {static_cast<std::uint16_t>(cut.coordinate - 1U),
                      static_cast<std::uint16_t>(cut.start + offset)},
                     spatial::BoundarySide::East, wall);
        } else {
            set_edge(tiles,
                     {static_cast<std::uint16_t>(cut.start + offset),
                      static_cast<std::uint16_t>(cut.coordinate - 1U)},
                     spatial::BoundarySide::South, wall);
        }
    }
    const auto midpoint = static_cast<std::uint16_t>(cut.start + cut.extent / 2U);
    if (cut.vertical) {
        set_edge(tiles, {static_cast<std::uint16_t>(cut.coordinate - 1U), midpoint},
                 spatial::BoundarySide::East, door);
    } else {
        set_edge(tiles, {midpoint, static_cast<std::uint16_t>(cut.coordinate - 1U)},
                 spatial::BoundarySide::South, door);
    }
    ++door_count;
}

void open_frontage(LocalTiles& tiles, const LocalHouse& house, rules::EdgeId door,
                   rules::EdgeId window, bool include_door, bool include_windows,
                   std::uint16_t& door_count, std::uint16_t& window_count) {
    const auto& rect = house.footprint;
    const bool horizontal = house.frontage == spatial::BoundarySide::North ||
                            house.frontage == spatial::BoundarySide::South;
    const auto start = horizontal ? rect.x : rect.y;
    const auto extent = horizontal ? rect.width : rect.height;
    const auto door_position = static_cast<std::uint16_t>(start + extent / 2U);
    auto edge_tile = [&](std::uint16_t position) {
        switch (house.frontage) {
            case spatial::BoundarySide::North:
                return LocalXY{position, rect.y};
            case spatial::BoundarySide::East:
                return LocalXY{static_cast<std::uint16_t>(rect.x + rect.width - 1U), position};
            case spatial::BoundarySide::South:
                return LocalXY{position, static_cast<std::uint16_t>(rect.y + rect.height - 1U)};
            case spatial::BoundarySide::West:
                return LocalXY{rect.x, position};
        }
        return LocalXY{};
    };
    if (include_door) {
        set_edge(tiles, edge_tile(door_position), house.frontage, door);
        ++door_count;
    }
    if (!include_windows) {
        return;
    }
    for (auto position = static_cast<std::uint16_t>(start + 2U); position + 1U < start + extent;
         position = static_cast<std::uint16_t>(position + 4U)) {
        if (position == door_position) {
            continue;
        }
        set_edge(tiles, edge_tile(position), house.frontage, window);
        ++window_count;
    }
}

[[nodiscard]] rules::LocalRoomKind room_kind(site::SiteZoning zoning,
                                             std::size_t room_index) noexcept {
    if (zoning == site::SiteZoning::Commercial) {
        return room_index == 0 ? rules::LocalRoomKind::Shop : rules::LocalRoomKind::Workshop;
    }
    return room_index % 4U == 0 ? rules::LocalRoomKind::Kitchen : rules::LocalRoomKind::Bedroom;
}

void build_floor(BuildingLocalSkeleton& result, const LocalSlowVars& slow,
                 std::uint16_t house_index, std::int8_t z, std::uint64_t seed,
                 const rules::Ruleset& ruleset) {
    const auto& config = ruleset.local_building_rules();
    auto& tiles = result.layers.at(z);
    const auto& house = result.houses[house_index];
    const auto door = slow.zoning == site::SiteZoning::Commercial ? config.commercial_door_edge
                                                                  : config.residential_door_edge;
    paint_outer_walls(tiles, house.footprint, config.wall_edge);
    const auto partition =
        spatial::partition_rect(house.footprint, seed ^ kRoomSalt,
                                {config.room_split_depth, config.room_cut_min_percent,
                                 config.room_cut_max_percent, config.room_min_extent, 0});
    for (std::size_t index = 0; index < partition.leaves.size(); ++index) {
        result.rooms.push_back(
            {partition.leaves[index], z, house_index, room_kind(slow.zoning, index)});
    }
    for (const auto& cut : partition.cuts) {
        paint_cut(tiles, cut, config.wall_edge, door, result.door_count);
    }
    open_frontage(tiles, house, door, config.window_edge, z == 0, z >= 0, result.door_count,
                  result.window_count);
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
    const auto middle_extent = static_cast<std::uint16_t>(kLocalHeight - 2U * middle_start);
    add_row(result.houses, spatial::BoundarySide::North, margin, outer_extent, margin, config,
            local_seed ^ kHouseSalt);
    add_row(result.houses, spatial::BoundarySide::South, margin, outer_extent,
            static_cast<std::uint16_t>(kLocalHeight - margin - depth), config,
            local_seed ^ kHouseSalt ^ UINT64_C(0x10));
    add_row(result.houses, spatial::BoundarySide::West, middle_start, middle_extent, margin, config,
            local_seed ^ kHouseSalt ^ UINT64_C(0x20));
    add_row(result.houses, spatial::BoundarySide::East, middle_start, middle_extent,
            static_cast<std::uint16_t>(kLocalWidth - margin - depth), config,
            local_seed ^ kHouseSalt ^ UINT64_C(0x30));

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

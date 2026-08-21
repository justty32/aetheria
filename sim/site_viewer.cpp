#include "sim/site_viewer.h"

#include "core/site/site_projection.h"
#include "core/site/site_wilderness.h"
#include "core/world/region_tiles.h"
#include "sim/debug_canvas.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace aetheria::sim {
namespace {

constexpr std::int32_t kTilePixels = 8;
constexpr std::uint32_t kImageExtent = site::kSiteWidth * kTilePixels + 1U;

[[nodiscard]] std::size_t tile_index(std::uint16_t x, std::uint16_t y) noexcept {
    return static_cast<std::size_t>(y) * site::kSiteWidth + x;
}

[[nodiscard]] DebugColor ground_color(rules::GroundId id, const rules::Ruleset& ruleset) {
    const auto* ground = ruleset.ground(id);
    if (ground == nullptr) {
        return {255, 0, 255};
    }
    if ((ground->flags & rules::kGroundWaterFlag) != 0) {
        return {42, 112, 180};
    }
    if (ground->id == "ground.grass") {
        return {112, 152, 82};
    }
    if (ground->id == "ground.sand") {
        return {210, 185, 118};
    }
    if (ground->id == "ground.mud") {
        return {116, 88, 64};
    }
    if (ground->id == "ground.tundra") {
        return {172, 186, 181};
    }
    return {139, 139, 139};
}

[[nodiscard]] std::optional<DebugColor> edge_color(rules::EdgeId id,
                                                   const rules::Ruleset& ruleset) {
    const auto* edge = ruleset.edge(id);
    if (edge == nullptr || edge->flags == 0) {
        return std::nullopt;
    }
    if ((edge->flags & rules::kEdgeGateFlag) != 0) {
        return DebugColor{255, 171, 37};
    }
    if ((edge->flags & rules::kEdgeTowerFlag) != 0) {
        return DebugColor{120, 48, 150};
    }
    if ((edge->flags & rules::kEdgeWallFlag) != 0) {
        return DebugColor{43, 32, 48};
    }
    if ((edge->flags & rules::kEdgeMoatFlag) != 0) {
        return DebugColor{36, 170, 210};
    }
    if ((edge->flags & rules::kEdgeRiverFlag) != 0) {
        return DebugColor{31, 96, 204};
    }
    if ((edge->flags & rules::kEdgeRoadFlag) != 0) {
        return DebugColor{111, 73, 48};
    }
    return DebugColor{230, 230, 230};
}

void paint_ground(DebugCanvas& canvas, const site::SiteSkeleton& skeleton,
                  const rules::Ruleset& ruleset) {
    for (std::uint16_t y = 0; y < site::kSiteHeight; ++y) {
        for (std::uint16_t x = 0; x < site::kSiteWidth; ++x) {
            canvas.fill_rect(x * kTilePixels, y * kTilePixels, kTilePixels, kTilePixels,
                             ground_color(skeleton.ground[tile_index(x, y)], ruleset));
        }
    }
}

void paint_roads(DebugCanvas& canvas, const site::SiteSkeleton& skeleton) {
    for (std::uint16_t y = 0; y < site::kSiteHeight; ++y) {
        for (std::uint16_t x = 0; x < site::kSiteWidth; ++x) {
            const auto index = tile_index(x, y);
            if (skeleton.roads[index] != 0) {
                canvas.fill_rect(x * kTilePixels, y * kTilePixels, kTilePixels, kTilePixels,
                                 {139, 94, 59});
            }
            if (skeleton.water[index] != 0) {
                canvas.fill_rect(x * kTilePixels, y * kTilePixels, kTilePixels, kTilePixels,
                                 {39, 115, 190});
            }
        }
    }
}

void paint_edge(DebugCanvas& canvas, std::uint16_t x, std::uint16_t y, spatial::BoundarySide side,
                DebugColor color) {
    const auto left = static_cast<std::int32_t>(x) * kTilePixels;
    const auto top = static_cast<std::int32_t>(y) * kTilePixels;
    switch (side) {
    case spatial::BoundarySide::North:
        canvas.draw_line(left, top, left + kTilePixels, top, 2, color);
        break;
    case spatial::BoundarySide::East:
        canvas.draw_line(left + kTilePixels, top, left + kTilePixels, top + kTilePixels, 2, color);
        break;
    case spatial::BoundarySide::South:
        canvas.draw_line(left, top + kTilePixels, left + kTilePixels, top + kTilePixels, 2, color);
        break;
    case spatial::BoundarySide::West:
        canvas.draw_line(left, top, left, top + kTilePixels, 2, color);
        break;
    }
}

void paint_edges(DebugCanvas& canvas, const std::vector<rules::EdgeId>& edges,
                 const rules::Ruleset& ruleset) {
    for (std::uint16_t y = 0; y < site::kSiteHeight; ++y) {
        for (std::uint16_t x = 0; x < site::kSiteWidth; ++x) {
            const auto index = tile_index(x, y);
            for (std::size_t side = 0; side < 4; ++side) {
                if ((side == 1 && x + 1U != site::kSiteWidth) ||
                    (side == 2 && y + 1U != site::kSiteHeight)) {
                    continue;
                }
                const auto color = edge_color(edges[index * 4U + side], ruleset);
                if (color.has_value()) {
                    paint_edge(canvas, x, y, static_cast<spatial::BoundarySide>(side), *color);
                }
            }
        }
    }
}

[[nodiscard]] DebugColor block_color(std::size_t index) noexcept {
    const auto value = static_cast<std::uint32_t>((index + 11U) * UINT32_C(2246822519));
    return {static_cast<std::uint8_t>(80U + (value & 0x8FU)),
            static_cast<std::uint8_t>(80U + ((value >> 8U) & 0x8FU)),
            static_cast<std::uint8_t>(80U + ((value >> 16U) & 0x8FU))};
}

void paint_blocks(DebugCanvas& canvas, const std::vector<site::SiteBlock>& blocks) {
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        const auto& block = blocks[index];
        canvas.fill_rect(block.origin.x * kTilePixels, block.origin.y * kTilePixels,
                         block.width * kTilePixels, block.height * kTilePixels, block_color(index));
    }
}

void paint_buildings(DebugCanvas& canvas, const site::SiteProceduralLayer& layer) {
    for (const auto& building : layer.buildings) {
        const DebugColor color = building.damage == site::ProceduralBuildingDamage::Intact
                                     ? DebugColor{67, 57, 64}
                                     : DebugColor{179, 66, 48};
        canvas.fill_rect(building.origin.x * kTilePixels + 2, building.origin.y * kTilePixels + 2,
                         building.width * kTilePixels - 3, building.height * kTilePixels - 3,
                         color);
    }
}

[[nodiscard]] site::SiteSlowVars city_slow(const rules::Ruleset& ruleset) {
    const auto road = *ruleset.find_edge("edge.road");
    return {*ruleset.find_terrain("terrain.grassland"),
            *ruleset.find_relief("relief.plain"),
            *ruleset.find_feature("feature.none"),
            3200,
            {road, road, road, road}};
}

[[nodiscard]] site::SiteFastVars city_fast() {
    site::SiteFastVars fast;
    fast.owner = static_cast<world::FactionId>(2);
    fast.settlement = world::SettlementTier::City;
    fast.population = 8000;
    fast.development_level = 20;
    fast.defense = 100;
    return fast;
}

void write_city(const site::SiteProceduralLayer& layer, const rules::Ruleset& ruleset,
                const std::filesystem::path& directory) {
    DebugCanvas ground{kImageExtent, kImageExtent, {24, 24, 28}};
    paint_ground(ground, layer.skeleton, ruleset);
    ground.write_png(directory / "site-city-ground.png");

    DebugCanvas blocks{kImageExtent, kImageExtent, {24, 24, 28}};
    paint_blocks(blocks, layer.skeleton.blocks);
    paint_roads(blocks, layer.skeleton);
    blocks.write_png(directory / "site-city-blocks.png");

    DebugCanvas roads{kImageExtent, kImageExtent, {24, 24, 28}};
    paint_ground(roads, layer.skeleton, ruleset);
    paint_roads(roads, layer.skeleton);
    paint_buildings(roads, layer);
    roads.write_png(directory / "site-city-roads.png");

    DebugCanvas edges{kImageExtent, kImageExtent, {24, 24, 28}};
    paint_ground(edges, layer.skeleton, ruleset);
    paint_roads(edges, layer.skeleton);
    paint_buildings(edges, layer);
    paint_edges(edges, layer.edges, ruleset);
    edges.write_png(directory / "site-city-edges.png");
}

[[nodiscard]] world::RegionTiles wilderness_region(const rules::Ruleset& ruleset) {
    world::RegionTiles region{5, 5};
    std::ranges::fill(region.base, *ruleset.find_terrain("terrain.grassland"));
    std::ranges::fill(region.relief, *ruleset.find_relief("relief.plain"));
    std::ranges::fill(region.feature, *ruleset.find_feature("feature.forest"));
    std::ranges::fill(region.edges, *ruleset.find_edge("edge.none"));
    for (std::int16_t y = 0; y < 5; ++y) {
        for (std::int16_t x = 0; x < 5; ++x) {
            region.elevation[region.index_of({x, y})] =
                static_cast<std::uint16_t>(1800 + x * 35 + y * 55);
        }
    }
    const auto road = *ruleset.find_edge("edge.road");
    const auto river = *ruleset.find_edge("edge.river");
    region.set_edge({2, 2}, {2, 1}, road);
    region.set_edge({2, 2}, {2, 3}, road);
    region.set_edge({2, 2}, {1, 2}, river);
    region.set_edge({2, 2}, {3, 2}, river);
    return region;
}

void paint_points(DebugCanvas& canvas, const std::vector<site::SiteXY>& points, DebugColor color,
                  std::int32_t size) {
    for (const auto point : points) {
        canvas.fill_rect(point.x * kTilePixels + (kTilePixels - size) / 2,
                         point.y * kTilePixels + (kTilePixels - size) / 2, size, size, color);
    }
}

void write_wilderness(const site::WildernessSite& wilderness, const rules::Ruleset& ruleset,
                      const std::filesystem::path& directory) {
    const auto& skeleton = wilderness.skeleton;
    DebugCanvas ground{kImageExtent, kImageExtent, {24, 24, 28}};
    paint_ground(ground, skeleton.terrain, ruleset);
    ground.write_png(directory / "site-wilderness-ground.png");

    DebugCanvas roads{kImageExtent, kImageExtent, {24, 24, 28}};
    paint_ground(roads, skeleton.terrain, ruleset);
    paint_roads(roads, skeleton.terrain);
    paint_edges(roads, skeleton.terrain.edges, ruleset);
    roads.write_png(directory / "site-wilderness-roads.png");

    DebugCanvas content{kImageExtent, kImageExtent, {24, 24, 28}};
    paint_ground(content, skeleton.terrain, ruleset);
    paint_roads(content, skeleton.terrain);
    paint_blocks(content, skeleton.ruin_structures);
    paint_points(content, skeleton.vegetation, {31, 112, 52}, 4);
    paint_points(content, wilderness.population.resource_points, {240, 185, 45}, 5);
    paint_points(content, wilderness.population.encounter_points, {224, 55, 76}, 5);
    paint_points(content, wilderness.population.traveler_points, {240, 240, 240}, 4);
    paint_points(content, skeleton.portals, {168, 75, 230}, 6);
    content.write_png(directory / "site-wilderness-content.png");
}

} // namespace

int run_gen_site(const rules::Ruleset& ruleset, std::uint64_t site_seed, std::string_view kind,
                 const std::filesystem::path& output_directory) {
    if (kind == "city") {
        const auto layer =
            site::populate(site::build_site_skeleton(city_slow(ruleset), site_seed, ruleset),
                           city_fast(), ruleset);
        write_city(layer, ruleset, output_directory);
        std::cout << "site kind=city site_seed=" << site_seed
                  << " blocks=" << layer.skeleton.blocks.size() << " roads="
                  << std::ranges::count_if(layer.skeleton.roads,
                                           [](auto value) { return value != 0; })
                  << " wall_edges=" << layer.wall_edges.size()
                  << " wall_gates=" << layer.wall_gates.size()
                  << " buildings=" << layer.buildings.size()
                  << " output=" << output_directory.string() << '\n';
        return 0;
    }
    if (kind == "wilderness") {
        const auto region = wilderness_region(ruleset);
        const world::RegionXY coordinate{2, 2};
        const auto slow =
            site::project_wilderness_slow_vars(region, coordinate, site_seed, 0, ruleset);
        auto fast = site::split_site_vars(region, coordinate).fast;
        fast.owner = static_cast<world::FactionId>(1);
        const auto wilderness = site::populate_wilderness(
            site::build_wilderness_skeleton(slow, site_seed, ruleset), fast, site_seed, ruleset);
        write_wilderness(wilderness, ruleset, output_directory);
        std::cout << "site kind=wilderness site_seed=" << site_seed
                  << " roads=" << wilderness.skeleton.road_path_count
                  << " rivers=" << wilderness.skeleton.river_path_count
                  << " vegetation=" << wilderness.skeleton.vegetation.size()
                  << " encounters=" << wilderness.population.encounter_points.size()
                  << " output=" << output_directory.string() << '\n';
        return 0;
    }
    throw std::invalid_argument{"gen site 的 kind 必須是 city 或 wilderness"};
}

} // namespace aetheria::sim

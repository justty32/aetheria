#include "sim/local_viewer.h"

#include "core/local/local_buildings.h"
#include "core/local/local_tiles.h"
#include "sim/debug_canvas.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace aetheria::sim {
namespace {

constexpr std::int32_t kTilePixels = 8;
constexpr std::uint32_t kImageExtent = local::kLocalWidth * kTilePixels + 1U;
constexpr site::SiteXY kParentTile{32, 32};

[[nodiscard]] std::size_t tile_index(std::uint16_t x, std::uint16_t y) noexcept {
    return static_cast<std::size_t>(y) * local::kLocalWidth + x;
}

[[nodiscard]] std::size_t site_tile_index(std::uint16_t x, std::uint16_t y) noexcept {
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
    if ((edge->flags & rules::kEdgeWindowFlag) != 0) {
        return DebugColor{56, 219, 255};
    }
    if ((edge->flags & rules::kEdgeWallFlag) != 0) {
        return DebugColor{43, 32, 48};
    }
    if ((edge->flags & rules::kEdgeRiverFlag) != 0) {
        return DebugColor{31, 96, 204};
    }
    if ((edge->flags & rules::kEdgeRoadFlag) != 0) {
        return DebugColor{111, 73, 48};
    }
    return DebugColor{230, 230, 230};
}

void paint_ground(DebugCanvas& canvas, const local::LocalTiles& tiles,
                  const rules::Ruleset& ruleset) {
    for (std::uint16_t y = 0; y < local::kLocalHeight; ++y) {
        for (std::uint16_t x = 0; x < local::kLocalWidth; ++x) {
            canvas.fill_rect(x * kTilePixels, y * kTilePixels, kTilePixels, kTilePixels,
                             ground_color(tiles.ground[tile_index(x, y)], ruleset));
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

void paint_edges(DebugCanvas& canvas, const local::LocalTiles& tiles,
                 const rules::Ruleset& ruleset) {
    for (std::uint16_t y = 0; y < local::kLocalHeight; ++y) {
        for (std::uint16_t x = 0; x < local::kLocalWidth; ++x) {
            const auto index = tile_index(x, y);
            for (std::size_t side = 0; side < 4; ++side) {
                if ((side == 1 && x + 1U != local::kLocalWidth) ||
                    (side == 2 && y + 1U != local::kLocalHeight)) {
                    continue;
                }
                const auto color = edge_color(tiles.edges[index * 4U + side], ruleset);
                if (color.has_value()) {
                    paint_edge(canvas, x, y, static_cast<spatial::BoundarySide>(side), *color);
                }
            }
        }
    }
}

[[nodiscard]] DebugColor room_color(std::size_t index) noexcept {
    const auto value = static_cast<std::uint32_t>((index + 1U) * UINT32_C(2654435761));
    return {static_cast<std::uint8_t>(90U + (value & 0x7FU)),
            static_cast<std::uint8_t>(90U + ((value >> 8U) & 0x7FU)),
            static_cast<std::uint8_t>(90U + ((value >> 16U) & 0x7FU))};
}

void paint_rooms(DebugCanvas& canvas, const local::BuildingLocalSkeleton& skeleton, std::int8_t z) {
    for (std::size_t index = 0; index < skeleton.rooms.size(); ++index) {
        const auto& room = skeleton.rooms[index];
        if (room.z != z) {
            continue;
        }
        canvas.fill_rect(room.footprint.x * kTilePixels, room.footprint.y * kTilePixels,
                         room.footprint.width * kTilePixels, room.footprint.height * kTilePixels,
                         room_color(index));
    }
}

void paint_occupants(DebugCanvas& canvas, const local::LocalTiles& tiles,
                     const local::BuildingLocalSkeleton* building, std::int8_t z) {
    std::vector<std::uint8_t> furniture(local::kLocalTileCount);
    if (building != nullptr) {
        for (const auto& item : building->furniture) {
            if (item.z == z) {
                furniture[tile_index(item.tile.x, item.tile.y)] = 1;
            }
        }
    }
    for (std::uint16_t y = 0; y < local::kLocalHeight; ++y) {
        for (std::uint16_t x = 0; x < local::kLocalWidth; ++x) {
            const auto index = tile_index(x, y);
            const auto left = static_cast<std::int32_t>(x) * kTilePixels;
            const auto top = static_cast<std::int32_t>(y) * kTilePixels;
            if (tiles.overlay[index] == local::OverlayId::Vegetation) {
                canvas.fill_rect(left + 2, top + 2, 4, 4, {31, 112, 52});
            } else if (tiles.overlay[index] == local::OverlayId::Stone) {
                canvas.fill_rect(left + 2, top + 2, 4, 4, {80, 88, 96});
            } else if (tiles.overlay[index] == local::OverlayId::ScatteredObject) {
                canvas.fill_rect(left + 2, top + 2, 4, 4, {212, 104, 42});
            } else if (tiles.overlay[index] == local::OverlayId::Road) {
                canvas.fill_rect(left + 1, top + 2, 7, 4, {132, 91, 58});
            } else if (tiles.overlay[index] == local::OverlayId::Stairs) {
                canvas.fill_rect(left + 1, top + 1, 6, 6, {245, 226, 75});
            }
            if (tiles.occupant[index] != 0) {
                canvas.fill_rect(left + 2, top + 2, 4, 4,
                                 furniture[index] != 0 ? DebugColor{32, 230, 220}
                                                       : DebugColor{246, 68, 146});
            }
        }
    }
}

[[nodiscard]] site::SiteProceduralLayer make_parent(site::SiteZoning zoning,
                                                    const rules::Ruleset& ruleset) {
    const auto grass = *ruleset.find_ground("ground.grass");
    const auto no_edge = *ruleset.find_edge("edge.none");
    site::SiteSkeleton skeleton;
    skeleton.ground.assign(site::kSiteTileCount, grass);
    skeleton.edges.assign(site::kSiteTileCount * 4U, no_edge);
    skeleton.elevation.resize(site::kSiteTileCount);
    skeleton.water.assign(site::kSiteTileCount, 0);
    skeleton.roads.assign(site::kSiteTileCount, 0);
    skeleton.buildable.assign(site::kSiteTileCount, 1);
    skeleton.city_center = {site::kSiteWidth / 2U, site::kSiteHeight / 2U};
    for (std::uint16_t y = 0; y < site::kSiteHeight; ++y) {
        for (std::uint16_t x = 0; x < site::kSiteWidth; ++x) {
            skeleton.elevation[static_cast<std::size_t>(y) * site::kSiteWidth + x] =
                static_cast<std::uint16_t>(900U + y * 7U + x * 3U);
        }
    }
    auto edges = skeleton.edges;
    site::SiteProceduralLayer parent{
        std::move(skeleton),
        std::move(edges),
        std::vector<site::SiteZoning>(site::kSiteTileCount, site::SiteZoning::Open),
        {},
        {},
        {},
        {},
        {},
        0};
    if (zoning != site::SiteZoning::Open) {
        parent.zoning[site_tile_index(kParentTile.x, kParentTile.y)] = zoning;
        const auto building =
            zoning == site::SiteZoning::Commercial ? "building.shop" : "building.row_house";
        parent.buildings.push_back({*ruleset.find_building(building),
                                    {31, 31},
                                    3,
                                    3,
                                    site::SiteBoundarySide::North,
                                    site::ProceduralBuildingDamage::Intact});
    } else {
        const auto road = *ruleset.find_edge("edge.road");
        const auto river = *ruleset.find_edge("edge.river");
        const auto set_edge = [&](site::SiteXY first, std::size_t first_side, site::SiteXY second,
                                  std::size_t second_side, rules::EdgeId edge) {
            parent.edges[site_tile_index(first.x, first.y) * 4U + first_side] = edge;
            parent.edges[site_tile_index(second.x, second.y) * 4U + second_side] = edge;
        };
        set_edge(kParentTile, 0, {32, 31}, 2, road);
        set_edge(kParentTile, 2, {32, 33}, 0, road);
        set_edge(kParentTile, 1, {33, 32}, 3, river);
        set_edge(kParentTile, 3, {31, 32}, 1, river);
    }
    return parent;
}

[[nodiscard]] site::SiteZoning parse_zoning(std::string_view value) {
    if (value == "residential") {
        return site::SiteZoning::Residential;
    }
    if (value == "commercial") {
        return site::SiteZoning::Commercial;
    }
    if (value == "open") {
        return site::SiteZoning::Open;
    }
    throw std::invalid_argument{"gen local 的 zoning 必須是 residential、commercial 或 open"};
}

[[nodiscard]] std::vector<std::int8_t>
selected_layers(std::string_view requested,
                const std::map<std::int8_t, local::LocalTiles>& layers) {
    if (requested == "all") {
        std::vector<std::int8_t> result;
        for (const auto& [z, tiles] : layers) {
            static_cast<void>(tiles);
            result.push_back(z);
        }
        return result;
    }
    std::size_t parsed{};
    const auto value = std::stoi(std::string{requested}, &parsed);
    if (parsed != requested.size() || value < -1 || value > 1 ||
        !layers.contains(static_cast<std::int8_t>(value))) {
        throw std::invalid_argument{"gen local 的 z 必須是現有的 -1、0、1 或 all"};
    }
    return {static_cast<std::int8_t>(value)};
}

[[nodiscard]] std::string z_name(std::int8_t z) { return z < 0 ? "m1" : (z > 0 ? "p1" : "0"); }

void write_local_layers(const std::map<std::int8_t, local::LocalTiles>& layers,
                        const local::BuildingLocalSkeleton* building, std::string_view zoning,
                        std::string_view z, const rules::Ruleset& ruleset,
                        const std::filesystem::path& directory) {
    for (const auto selected : selected_layers(z, layers)) {
        const auto& tiles = layers.at(selected);
        const auto stem = "local-" + std::string{zoning} + "-z" + z_name(selected);

        DebugCanvas ground{kImageExtent, kImageExtent, {24, 24, 28}};
        paint_ground(ground, tiles, ruleset);
        ground.write_png(directory / (stem + "-ground.png"));

        DebugCanvas edges{kImageExtent, kImageExtent, {24, 24, 28}};
        paint_ground(edges, tiles, ruleset);
        paint_edges(edges, tiles, ruleset);
        edges.write_png(directory / (stem + "-edges.png"));

        DebugCanvas rooms{kImageExtent, kImageExtent, {24, 24, 28}};
        if (building != nullptr) {
            paint_rooms(rooms, *building, selected);
        } else {
            paint_ground(rooms, tiles, ruleset);
        }
        paint_edges(rooms, tiles, ruleset);
        rooms.write_png(directory / (stem + "-rooms.png"));

        DebugCanvas occupants{kImageExtent, kImageExtent, {24, 24, 28}};
        paint_ground(occupants, tiles, ruleset);
        paint_occupants(occupants, tiles, building, selected);
        paint_edges(occupants, tiles, ruleset);
        occupants.write_png(directory / (stem + "-occupants.png"));
    }
}

} // namespace

int run_gen_local(const rules::Ruleset& ruleset, std::uint64_t site_seed, std::string_view zoning,
                  std::string_view z, const std::filesystem::path& output_directory) {
    const auto parsed_zoning = parse_zoning(zoning);
    auto parent = make_parent(parsed_zoning, ruleset);
    const auto feature = *ruleset.find_feature(
        parsed_zoning == site::SiteZoning::Open ? "feature.forest" : "feature.none");
    const auto slow =
        local::project_local_slow_vars(parent, kParentTile, site_seed, feature, ruleset);
    if (parsed_zoning == site::SiteZoning::Open) {
        const auto generated = local::build_open_local_skeleton(slow, site_seed, ruleset);
        const std::map<std::int8_t, local::LocalTiles> layers{{0, generated.tiles}};
        write_local_layers(layers, nullptr, zoning, z, ruleset, output_directory);
        std::cout << "local route=B site_seed=" << site_seed << " zoning=" << zoning
                  << " layers=1 scatter=" << generated.scatter_count
                  << " objects=" << generated.object_count
                  << " output=" << output_directory.string() << '\n';
        return 0;
    }

    auto generated = local::build_building_local_skeleton(slow, site_seed, ruleset);
    const auto furniture_count = generated.furniture.size();
    const auto room_count = generated.rooms.size();
    const auto door_count = generated.door_count;
    const auto house_count = generated.houses.size();
    auto rendered = generated;
    for (std::uint16_t house = 0; house < rendered.houses.size(); ++house) {
        local::materialize_ambient_residents(rendered, house, site_seed);
    }
    write_local_layers(rendered.layers, &rendered, zoning, z, ruleset, output_directory);
    std::cout << "local route=A site_seed=" << site_seed << " zoning=" << zoning
              << " houses=" << house_count << " rooms=" << room_count << " doors=" << door_count
              << " windows=" << generated.window_count << " furniture=" << furniture_count
              << " residents=" << generated.resident_statistics
              << " layers=" << generated.layers.size() << " output=" << output_directory.string()
              << '\n';
    return 0;
}

} // namespace aetheria::sim

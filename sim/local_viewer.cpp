#include "sim/local_viewer.h"
#include "sim/local_viewer_detail.h"

#include "core/local/local_buildings.h"
#include "core/local/local_tiles.h"
#include "core/local/local_underground.h"
#include "sim/debug_canvas.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace aetheria::sim {
using namespace detail::local_viewer;
namespace {

constexpr site::SiteXY kParentTile{32, 32};

[[nodiscard]] std::size_t site_tile_index(std::uint16_t x, std::uint16_t y) noexcept {
    return static_cast<std::size_t>(y) * site::kSiteWidth + x;
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
    throw std::invalid_argument{
        "gen local 的 zoning 必須是 residential、commercial、open、mine、dungeon "
        "或 ruin"};
}

[[nodiscard]] bool is_underground(std::string_view value) noexcept {
    return value == "mine" || value == "dungeon" || value == "ruin";
}

[[nodiscard]] site::SiteProceduralLayer make_underground_parent(std::string_view kind,
                                                                const rules::Ruleset& ruleset) {
    auto parent = make_parent(site::SiteZoning::Open, ruleset);
    const auto id = "building." + std::string{kind} + "_entrance";
    const auto structure = ruleset.find_building(id);
    if (!structure.has_value()) {
        throw std::runtime_error{"gen local 找不到地下 structure def：" + id};
    }
    parent.buildings.push_back({*structure,
                                {31, 31},
                                3,
                                3,
                                site::SiteBoundarySide::North,
                                site::ProceduralBuildingDamage::Intact});
    return parent;
}

} // namespace

int run_gen_local(const rules::Ruleset& ruleset, std::uint64_t site_seed, std::string_view zoning,
                  std::string_view z, const std::filesystem::path& output_directory) {
    if (is_underground(zoning)) {
        const auto parent = make_underground_parent(zoning, ruleset);
        const auto feature = *ruleset.find_feature("feature.ancient_foundation");
        const auto slow =
            local::project_local_slow_vars(parent, kParentTile, site_seed, feature, ruleset);
        const auto generated = local::build_underground_local_skeleton(slow, site_seed, ruleset);
        write_local_layers(generated.layers, nullptr, &generated, zoning, z, ruleset,
                           output_directory);
        std::cout << "local route=C site_seed=" << site_seed << " zoning=" << zoning
                  << " depth=" << static_cast<unsigned>(generated.depth)
                  << " excavated=" << generated.excavated_count
                  << " rooms=" << generated.rooms.size()
                  << " corridors=" << generated.corridors.size()
                  << " vertical_links=" << generated.vertical_links.size()
                  << " output=" << output_directory.string() << '\n';
        return 0;
    }
    const auto parsed_zoning = parse_zoning(zoning);
    auto parent = make_parent(parsed_zoning, ruleset);
    const auto feature = *ruleset.find_feature(
        parsed_zoning == site::SiteZoning::Open ? "feature.forest" : "feature.none");
    const auto slow =
        local::project_local_slow_vars(parent, kParentTile, site_seed, feature, ruleset);
    if (parsed_zoning == site::SiteZoning::Open) {
        const auto generated = local::build_open_local_skeleton(slow, site_seed, ruleset);
        const std::map<std::int8_t, local::LocalTiles> layers{{0, generated.tiles}};
        write_local_layers(layers, nullptr, nullptr, zoning, z, ruleset, output_directory);
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
    write_local_layers(rendered.layers, &rendered, nullptr, zoning, z, ruleset, output_directory);
    std::cout << "local route=A site_seed=" << site_seed << " zoning=" << zoning
              << " houses=" << house_count << " rooms=" << room_count << " doors=" << door_count
              << " windows=" << generated.window_count << " furniture=" << furniture_count
              << " residents=" << generated.resident_statistics
              << " layers=" << generated.layers.size() << " output=" << output_directory.string()
              << '\n';
    return 0;
}

} // namespace aetheria::sim

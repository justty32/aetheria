#pragma once

// 城建循環測試共用的全可建 Site、單格 Region 與兩種配置 fixture。

#include "core/site/site_build_loop.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace aetheria::tests {

inline constexpr std::uint32_t kBuildRegionId = 7;
inline constexpr world::RegionXY kBuildCoordinate{0, 0};
inline constexpr auto kBuildRegionKey =
    zone::child_key(zone::kRootZone, kBuildRegionId, 0);
inline constexpr auto kBuildSiteKey = zone::child_key(kBuildRegionKey, 0, 0);

struct BuildFixture {
    zone::Zone region{kBuildRegionKey};
    zone::Zone site{kBuildSiteKey};
};

[[nodiscard]] inline BuildFixture build_fixture() {
    const auto& ruleset = test_ruleset();
    BuildFixture fixture;
    world::RegionTiles tiles{1, 1};
    tiles.base[0] = *ruleset.find_terrain("terrain.grassland");
    tiles.relief[0] = *ruleset.find_relief("relief.plain");
    tiles.feature[0] = *ruleset.find_feature("feature.none");
    std::ranges::fill(tiles.edges, *ruleset.find_edge("edge.none"));
    tiles.settlement[0] = world::SettlementTier::Town;
    tiles.site[0].has_live_site = true;
    tiles.site[0].lod = zone::LodLevel::Coarse;
    auto& region_tiles =
        std::get<zone::RegionPayload>(fixture.region.payload)
            .layers.emplace(0, std::move(tiles))
            .first->second;
    const auto region_meta = *fixture.region.reg.view<zone::ZoneMeta>().begin();
    fixture.region.reg.emplace<world::TurnClock>(region_meta);

    auto& procedural = std::get<zone::SitePayload>(fixture.site.payload).layers.procedural;
    const auto ground = *ruleset.find_ground("ground.grass");
    const auto edge = *ruleset.find_edge("edge.none");
    procedural.skeleton.ground.assign(site::kSiteTileCount, ground);
    procedural.skeleton.edges.assign(site::kSiteTileCount * 4U, edge);
    procedural.skeleton.elevation.assign(site::kSiteTileCount, 100);
    procedural.skeleton.water.assign(site::kSiteTileCount, 0);
    procedural.skeleton.roads.assign(site::kSiteTileCount, 0);
    procedural.skeleton.buildable.assign(site::kSiteTileCount, 1);
    procedural.skeleton.city_center = {32, 32};
    procedural.edges = procedural.skeleton.edges;
    procedural.zoning.assign(site::kSiteTileCount, site::SiteZoning::Open);
    if (!procedural.valid_layout()) {
        throw std::runtime_error{"城建測試 fixture 的程序層 layout 無效"};
    }
    std::get<zone::SitePayload>(fixture.site.payload).layers.persistent.buildings.push_back(
        {{1, 1}, site::BuildingType::SettlementHall, site::BuildingState::Active});
    site::enter_full_site(fixture.site, region_tiles, kBuildCoordinate);
    return fixture;
}

inline void queue_layout(zone::Zone& site_zone, bool good) {
    const auto& ruleset = test_ruleset();
    site::start_construction(site_zone, "city.house", {10, 10}, ruleset);
    site::start_construction(site_zone, "city.farm", {40, 40}, ruleset);
    if (good) {
        site::start_construction(site_zone, "city.square", {12, 10}, ruleset);
        site::start_construction(site_zone, "city.workshop", {20, 20}, ruleset);
        site::start_construction(site_zone, "city.mine", {23, 20}, ruleset);
    } else {
        site::start_construction(site_zone, "city.workshop", {12, 10}, ruleset);
        site::start_construction(site_zone, "city.square", {30, 10}, ruleset);
        site::start_construction(site_zone, "city.mine", {30, 20}, ruleset);
    }
}

[[nodiscard]] inline std::uint64_t production_at(const BuildFixture& fixture) {
    const auto& tiles = std::get<zone::RegionPayload>(fixture.region.payload).layers.at(0);
    return tiles.reduction_value<world::ProductionStockReduction>(kBuildCoordinate);
}

}  // namespace aetheria::tests

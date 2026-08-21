#pragma once

// Site 往返測試共用的 Region fixture、非預設持久層存檔與磁碟世界雜湊 helper。

#include "core/site/site_materialize.h"
#include "core/site/site_reduction.h"
#include "core/zone/file_zone_store.h"
#include "sim/world_hash.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/zone/zone_test_support.h"

#include <algorithm>
#include <cstdint>

#include <gtest/gtest.h>

namespace aetheria::tests {

inline constexpr std::uint64_t kRoundTripWorldSeed = UINT64_C(0xA37E1222);
inline constexpr std::uint32_t kRoundTripRegionId = 7;
inline constexpr world::RegionXY kRoundTripCoordinate{4, 7};
inline constexpr auto kRoundTripRegionKey =
    zone::child_key(zone::kRootZone, kRoundTripRegionId, 0);
inline constexpr auto kRoundTripSiteKey = zone::child_key(kRoundTripRegionKey, 4, 7);

[[nodiscard]] inline world::RegionTiles round_trip_region() {
    const auto& ruleset = test_ruleset();
    world::RegionTiles tiles{8, 8};
    std::ranges::fill(tiles.base, *ruleset.find_terrain("terrain.grassland"));
    std::ranges::fill(tiles.relief, *ruleset.find_relief("relief.plain"));
    std::ranges::fill(tiles.feature, *ruleset.find_feature("feature.none"));
    std::ranges::fill(tiles.edges, *ruleset.find_edge("edge.none"));
    const auto index = tiles.index_of(kRoundTripCoordinate);
    tiles.owner[index] = static_cast<world::FactionId>(2);
    tiles.settlement[index] = world::SettlementTier::Town;
    site::SiteLayers reduced;
    reduced.persistent.buildings.push_back(
        {{1, 1}, site::BuildingType::SettlementHall, site::BuildingState::Idle});
    site::ReductionTable::apply(tiles, kRoundTripCoordinate,
                                site::ReductionTable::reduce(reduced));
    return tiles;
}

[[nodiscard]] inline site::SiteProceduralLayer prepare_idle_digest(
    zone::FileZoneStore& store, world::RegionTiles& tiles) {
    store.save(zone::Zone{zone::kRootZone});
    auto materialized = site::materialize_site_zone(tiles, kRoundTripCoordinate,
                                                    kRoundTripWorldSeed, kRoundTripRegionId,
                                                    test_ruleset());
    auto& layers = std::get<zone::SitePayload>(materialized.payload).layers;
    EXPECT_FALSE(layers.procedural.buildings.empty());
    EXPECT_TRUE(std::ranges::any_of(layers.procedural.block_zoning,
                                    [](site::SiteZoning zone) {
                                        return zone != site::SiteZoning::Open;
                                    }));
    layers.persistent.buildings.front().state = site::BuildingState::Idle;
    auto expected_procedural = layers.procedural;
    store.save(materialized);
    auto& site_state = tiles.site.at(tiles.index_of(kRoundTripCoordinate));
    site_state.lod = zone::LodLevel::Absent;
    site_state.has_live_site = false;
    store.write_manifest(zone::SaveManifest{.world_seed = kRoundTripWorldSeed});
    return expected_procedural;
}

[[nodiscard]] inline std::uint64_t disk_world_hash(const TemporaryDirectory& directory) {
    const auto report = sim::world_state_hash(directory.path(), test_ruleset());
    EXPECT_EQ(report.zone_count, 2U);
    return report.hash;
}

[[nodiscard]] inline site::BuildingState building_state(zone::ZoneManager& manager,
                                                        zone::ZoneHandle handle) {
    auto result = site::BuildingState::Active;
    const bool borrowed = manager.with(handle, [&](const zone::Zone& materialized) {
        const auto& buildings =
            std::get<zone::SitePayload>(materialized.payload).layers.persistent.buildings;
        ASSERT_EQ(buildings.size(), 1U);
        result = buildings.front().state;
    });
    EXPECT_TRUE(borrowed);
    return result;
}

}  // namespace aetheria::tests

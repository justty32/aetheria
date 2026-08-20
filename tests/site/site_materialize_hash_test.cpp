#include "core/site/site_materialize.h"
#include "core/zone/file_zone_store.h"
#include "sim/world_hash.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/zone/zone_test_support.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using aetheria::site::BuildingState;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::world::RegionTiles;
using aetheria::world::RegionXY;

constexpr std::uint64_t kWorldSeed = UINT64_C(0xA37E1222);
constexpr std::uint32_t kRegionId = 7;
constexpr RegionXY kCoordinate{4, 7};

[[nodiscard]] RegionTiles sample_region() {
    const auto& ruleset = test_ruleset();
    RegionTiles tiles{8, 8};
    std::ranges::fill(tiles.base, *ruleset.find_terrain("terrain.grassland"));
    std::ranges::fill(tiles.relief, *ruleset.find_relief("relief.plain"));
    std::ranges::fill(tiles.feature, *ruleset.find_feature("feature.none"));
    std::ranges::fill(tiles.edges, *ruleset.find_edge("edge.none"));
    const auto index = tiles.index_of(kCoordinate);
    tiles.owner[index] = static_cast<aetheria::world::FactionId>(2);
    tiles.settlement[index] = aetheria::world::SettlementTier::Town;
    return tiles;
}

void save_materialization(const std::filesystem::path& directory, BuildingState state) {
    aetheria::zone::FileZoneStore store{directory, test_ruleset()};
    store.save(aetheria::zone::Zone{aetheria::zone::kRootZone});
    auto site = aetheria::site::materialize_site_zone(sample_region(), kCoordinate, kWorldSeed,
                                                      kRegionId, test_ruleset());
    auto& persistent = std::get<aetheria::zone::SitePayload>(site.payload).layers.persistent;
    ASSERT_EQ(persistent.buildings.size(), 1U);
    persistent.buildings.front().state = state;
    store.save(site);
    store.write_manifest(aetheria::zone::SaveManifest{.world_seed = kWorldSeed});
}

TEST(SiteMaterialize, WorldHashMatchesTwiceAndChangesWithBuildingState) {
    TemporaryDirectory first_directory;
    TemporaryDirectory second_directory;
    TemporaryDirectory changed_directory;
    save_materialization(first_directory.path(), BuildingState::Active);
    save_materialization(second_directory.path(), BuildingState::Active);
    save_materialization(changed_directory.path(), BuildingState::Idle);

    const auto first = aetheria::sim::world_state_hash(first_directory.path(), test_ruleset());
    const auto second = aetheria::sim::world_state_hash(second_directory.path(), test_ruleset());
    const auto changed = aetheria::sim::world_state_hash(changed_directory.path(), test_ruleset());

    std::cout << "site_materialize_first world_hash=" << first.hash << '\n'
              << "site_materialize_second world_hash=" << second.hash << '\n'
              << "site_building_state_changed world_hash=" << changed.hash << '\n';
    EXPECT_EQ(first.zone_count, 2U);
    EXPECT_EQ(second.zone_count, 2U);
    EXPECT_EQ(first.hash, second.hash);
    EXPECT_NE(first.hash, changed.hash);
}

TEST(SiteMaterialize, FitsThirtyMillisecondBudget) {
    const auto tiles = sample_region();
    static_cast<void>(aetheria::site::materialize_site_zone(tiles, kCoordinate, kWorldSeed,
                                                            kRegionId, test_ruleset()));
    const auto start = std::chrono::steady_clock::now();
    const auto site = aetheria::site::materialize_site_zone(tiles, kCoordinate, kWorldSeed,
                                                            kRegionId, test_ruleset());
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const auto milliseconds = std::chrono::duration<double, std::milli>{elapsed}.count();

#ifdef NDEBUG
    constexpr auto build_kind = "Release";
#else
    constexpr auto build_kind = "Debug";
#endif
    std::cout << "site_materialize_" << build_kind << "_ms=" << milliseconds << '\n';
    EXPECT_EQ(site.lod, aetheria::zone::LodLevel::Coarse);
    EXPECT_LT(elapsed, std::chrono::milliseconds{30});
}

}  // namespace

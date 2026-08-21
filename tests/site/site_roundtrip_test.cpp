#include "tests/site/site_roundtrip_test_support.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::site::BuildingState;
using aetheria::tests::building_state;
using aetheria::tests::disk_world_hash;
using aetheria::tests::kRoundTripCoordinate;
using aetheria::tests::kRoundTripRegionId;
using aetheria::tests::kRoundTripSiteKey;
using aetheria::tests::kRoundTripWorldSeed;
using aetheria::tests::prepare_idle_digest;
using aetheria::tests::round_trip_region;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::zone::FileZoneStore;
using aetheria::zone::Zone;
using aetheria::zone::ZoneManager;

TEST(SiteRoundTrip, ThreeColdRoundTripsKeepEveryHashAndRecomputeProceduralLayer) {
    TemporaryDirectory directory;
    auto tiles = round_trip_region();
    FileZoneStore store{directory.path(), test_ruleset()};
    const auto expected_procedural = prepare_idle_digest(store, tiles);
    ZoneManager manager{store};
    std::array<std::uint64_t, 6> hashes{};
    std::vector<double> expand_milliseconds;
    std::vector<double> collapse_milliseconds;

    for (std::size_t round = 0; round < 3; ++round) {
        ASSERT_FALSE(manager.get(kRoundTripSiteKey).has_value())
            << "round " << round << " 必須從未載入的 L_ABSENT 冷展開";
        const auto expand_start = std::chrono::steady_clock::now();
        const auto handle = aetheria::site::rematerialize_site_zone(
            manager, tiles, kRoundTripCoordinate, kRoundTripWorldSeed, kRoundTripRegionId,
            test_ruleset());
        expand_milliseconds.push_back(
            std::chrono::duration<double, std::milli>{std::chrono::steady_clock::now() -
                                                       expand_start}
                .count());
        EXPECT_EQ(building_state(manager, handle), BuildingState::Idle);
        manager.save_all();
        hashes[round * 2] = disk_world_hash(directory);

        if (round == 0) {
            EXPECT_THROW(static_cast<void>(aetheria::site::rematerialize_site_zone(
                             manager, tiles, kRoundTripCoordinate, kRoundTripWorldSeed,
                             kRoundTripRegionId, test_ruleset())),
                         std::logic_error);
        }
        if (round == 2) {
            ASSERT_TRUE(manager.with(handle, [&](const Zone& materialized) {
                const auto& recomputed =
                    std::get<aetheria::zone::SitePayload>(materialized.payload).layers.procedural;
                EXPECT_EQ(recomputed.skeleton, expected_procedural.skeleton);
                EXPECT_EQ(recomputed.zoning, expected_procedural.zoning);
                EXPECT_EQ(aetheria::site::hash_site_fill(recomputed),
                          aetheria::site::hash_site_fill(expected_procedural));
            }));
        }

        if (round == 1) {
            ASSERT_TRUE(manager.with(handle, [](Zone& materialized) {
                auto& procedural =
                    std::get<aetheria::zone::SitePayload>(materialized.payload).layers.procedural;
                procedural.skeleton.ground.clear();
                procedural.zoning.clear();
                procedural.block_zoning.clear();
                procedural.buildings.clear();
                ASSERT_FALSE(procedural.valid_layout());
            }));
        }

        const auto collapse_start = std::chrono::steady_clock::now();
        aetheria::site::collapse_site_zone(manager, handle, tiles, kRoundTripCoordinate);
        collapse_milliseconds.push_back(
            std::chrono::duration<double, std::milli>{std::chrono::steady_clock::now() -
                                                       collapse_start}
                .count());
        ASSERT_FALSE(manager.get(kRoundTripSiteKey).has_value());
        hashes[round * 2 + 1] = disk_world_hash(directory);

        auto disk_only = store.load(kRoundTripSiteKey);
        ASSERT_NE(disk_only, nullptr);
        const auto& disk_layers =
            std::get<aetheria::zone::SitePayload>(disk_only->payload).layers;
        EXPECT_TRUE(disk_layers.procedural.skeleton.ground.empty());
        EXPECT_TRUE(disk_layers.procedural.zoning.empty());
        EXPECT_TRUE(disk_layers.procedural.block_zoning.empty());
        EXPECT_TRUE(disk_layers.procedural.buildings.empty());
        ASSERT_EQ(disk_layers.persistent.buildings.size(), 1U);
        EXPECT_EQ(disk_layers.persistent.buildings.front().state, BuildingState::Idle);
    }

    EXPECT_TRUE(std::ranges::all_of(hashes, [&](const auto hash) { return hash == hashes.front(); }));
    ASSERT_FALSE(manager.get(kRoundTripSiteKey).has_value());

    const auto max_expand = *std::ranges::max_element(expand_milliseconds);
    const auto max_collapse = *std::ranges::max_element(collapse_milliseconds);
    std::cout << "site_roundtrip_hashes";
    for (const auto hash : hashes) {
        std::cout << ' ' << hash;
    }
    std::cout << '\n'
              << "site_roundtrip_building_state="
              << static_cast<unsigned>(BuildingState::Idle) << " (Idle)\n"
              << "site_roundtrip_cold_assertions=3 procedural_disk_empty=1 "
                 "procedural_fill_nonempty=1 procedural_f3_f5_nonempty=1 "
                 "procedural_recomputed_after_cache_corruption=1\n"
              << "site_rematerialize_Debug_max_ms=" << max_expand << '\n'
              << "site_collapse_Debug_max_ms=" << max_collapse << '\n';
    EXPECT_LT(max_expand, 30.0);
    EXPECT_LT(max_collapse, 30.0);
}

}  // namespace

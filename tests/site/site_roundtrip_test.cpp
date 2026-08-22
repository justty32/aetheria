#include "tests/site/site_roundtrip_test_support.h"
#include "tests/support/performance.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

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

TEST(SiteRoundTrip, ZoneManagerAcquireRunsThreeStepRematerializeAndKeepsBuildingTile) {
    TemporaryDirectory directory;
    auto tiles = round_trip_region();
    FileZoneStore store{directory.path(), test_ruleset()};
    static_cast<void>(prepare_idle_digest(store, tiles));
    const auto disk_only = store.load(kRoundTripSiteKey);
    ASSERT_NE(disk_only, nullptr);
    const auto& disk_layers =
        std::get<aetheria::zone::SitePayload>(disk_only->payload).layers;
    ASSERT_TRUE(disk_layers.procedural.skeleton.ground.empty());
    ASSERT_EQ(disk_layers.persistent.buildings.size(), 1U);
    const auto saved_tile = disk_layers.persistent.buildings.front().tile;

    ZoneManager manager{store};
    const auto acquired = manager.acquire(
        kRoundTripSiteKey,
        [&](aetheria::zone::ZoneKey key, std::unique_ptr<Zone> persistent) {
            if (key != kRoundTripSiteKey || persistent == nullptr) {
                return std::unique_ptr<Zone>{};
            }
            auto site = aetheria::site::rematerialize_site_zone(
                std::move(*persistent), tiles, kRoundTripCoordinate, kRoundTripWorldSeed,
                kRoundTripRegionId, aetheria::time::Tick{}, test_ruleset());
            return std::make_unique<Zone>(std::move(site));
        });
    ASSERT_TRUE(acquired.has_value());
    ASSERT_TRUE(manager.with(*acquired, [&](const Zone& site) {
        const auto& layers = std::get<aetheria::zone::SitePayload>(site.payload).layers;
        EXPECT_EQ(layers.procedural.skeleton.ground.size(), aetheria::site::kSiteTileCount);
        ASSERT_EQ(layers.persistent.buildings.size(), 1U);
        EXPECT_EQ(layers.persistent.buildings.front().tile, saved_tile);
        EXPECT_EQ(layers.persistent.buildings.front().state, BuildingState::Idle);
        std::cout << "site_acquire raw_tiles=0 rematerialized_tiles="
                  << layers.procedural.skeleton.ground.size() << " persistent_objects=1 at="
                  << saved_tile.x << ',' << saved_tile.y << '\n';
    }));
}

TEST(SiteRoundTrip, ThreeColdRoundTripsKeepEveryHashAndRecomputeProceduralLayer) {
    TemporaryDirectory directory;
    auto tiles = round_trip_region();
    FileZoneStore store{directory.path(), test_ruleset()};
    const auto expected_procedural = prepare_idle_digest(store, tiles);
    ZoneManager manager{store};
    std::array<std::uint64_t, 6> hashes{};

    for (std::size_t round = 0; round < 3; ++round) {
        ASSERT_FALSE(manager.get(kRoundTripSiteKey).has_value())
            << "round " << round << " 必須從未載入的 L_ABSENT 冷展開";
        const auto handle = aetheria::site::rematerialize_site_zone(
            manager, tiles, kRoundTripCoordinate, kRoundTripWorldSeed, kRoundTripRegionId,
            test_ruleset());
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

        aetheria::site::collapse_site_zone(manager, handle, tiles, kRoundTripCoordinate);
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

    const auto measure_cycle = [&] {
        if (manager.get(kRoundTripSiteKey).has_value()) {
            throw std::logic_error{"效能量測的冷展開起點仍在記憶體"};
        }
        const auto expand_start = std::chrono::steady_clock::now();
        const auto handle = aetheria::site::rematerialize_site_zone(
            manager, tiles, kRoundTripCoordinate, kRoundTripWorldSeed, kRoundTripRegionId,
            test_ruleset());
        const auto expand = std::chrono::duration<double, std::milli>{
                                std::chrono::steady_clock::now() - expand_start}
                                .count();
        const auto collapse_start = std::chrono::steady_clock::now();
        aetheria::site::collapse_site_zone(manager, handle, tiles, kRoundTripCoordinate);
        const auto collapse = std::chrono::duration<double, std::milli>{
                                  std::chrono::steady_clock::now() - collapse_start}
                                  .count();
        return std::array{expand, collapse};
    };
    static_cast<void>(measure_cycle());
    double minimum_expand = std::numeric_limits<double>::max();
    double minimum_collapse = std::numeric_limits<double>::max();
    for (std::size_t sample = 0; sample < aetheria::tests::kPerformanceSampleCount; ++sample) {
        const auto measured = measure_cycle();
        minimum_expand = std::min(minimum_expand, measured[0]);
        minimum_collapse = std::min(minimum_collapse, measured[1]);
    }
#ifdef NDEBUG
    constexpr auto build_kind = "Release";
#else
    constexpr auto build_kind = "Debug";
#endif
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
              << "site_rematerialize_" << build_kind << "_min_of_5_ms=" << minimum_expand
              << '\n'
              << "site_collapse_" << build_kind << "_min_of_5_ms=" << minimum_collapse << '\n';
    EXPECT_LT(minimum_expand, 30.0);
    EXPECT_LT(minimum_collapse, 30.0);
}

}  // namespace

#include "core/site/site_streaming.h"
#include "core/site/site_wilderness.h"
#include "tests/site/site_build_loop_test_support.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>

#include <gtest/gtest.h>

namespace {

using aetheria::tests::test_ruleset;

constexpr std::uint32_t kStreamingRegionId = 19;
constexpr auto kStreamingRegionKey =
    aetheria::zone::child_key(aetheria::zone::kRootZone, kStreamingRegionId, 0);
constexpr std::uint64_t kStreamingWorldSeed = UINT64_C(0x5EED77112233);
constexpr aetheria::world::RegionXY kLeftCenter{4, 4};
constexpr aetheria::world::RegionXY kRightCenter{5, 4};

struct StreamingFixture {
    aetheria::zone::Zone region{kStreamingRegionKey};
    aetheria::zone::InMemoryZoneStore store{test_ruleset()};
    aetheria::zone::ZoneManager manager{store};
    std::unique_ptr<aetheria::site::SiteStreamingCoordinator> coordinator;

    explicit StreamingFixture(
        aetheria::world::SettlementTier settlement =
            aetheria::world::SettlementTier::Town) {
        aetheria::world::RegionTiles tiles{10, 10};
        const auto& ruleset = test_ruleset();
        std::ranges::fill(tiles.base, *ruleset.find_terrain("terrain.grassland"));
        std::ranges::fill(tiles.relief, *ruleset.find_relief("relief.plain"));
        std::ranges::fill(
            tiles.feature,
            *ruleset.find_feature(settlement == aetheria::world::SettlementTier::None
                                      ? "feature.forest"
                                      : "feature.none"));
        std::ranges::fill(tiles.edges, *ruleset.find_edge("edge.none"));
        std::ranges::fill(tiles.settlement, settlement);
        std::get<aetheria::zone::RegionPayload>(region.payload)
            .layers.emplace(0, std::move(tiles));
        const auto meta = *region.reg.view<aetheria::zone::ZoneMeta>().begin();
        region.reg.emplace<aetheria::world::TurnClock>(meta);
        coordinator = std::make_unique<aetheria::site::SiteStreamingCoordinator>(
            ruleset, store, manager, region, 0, kStreamingWorldSeed);
    }
};

struct ChurnCounts {
    std::uint64_t loads{};
    std::uint64_t unloads{};
};

[[nodiscard]] ChurnCounts pace_boundary(std::uint32_t crossings) {
    StreamingFixture fixture;
    fixture.coordinator->player_crossed_tile(kLeftCenter);
    static_cast<void>(fixture.coordinator->finish_turn());
    for (std::uint32_t crossing = 0; crossing < crossings; ++crossing) {
        fixture.coordinator->player_crossed_tile(crossing % 2U == 0 ? kRightCenter
                                                                    : kLeftCenter);
        static_cast<void>(fixture.coordinator->finish_turn());
    }
    return {fixture.coordinator->load_count(), fixture.coordinator->unload_count()};
}

TEST(SiteStreaming, PlayerFieldHasNineFullAndSixteenCoarseAndBatchesOneXun) {
    StreamingFixture fixture;
    fixture.coordinator->player_crossed_tile(kLeftCenter);
    EXPECT_EQ(fixture.coordinator->lod_counts(), (aetheria::site::StreamingLodCounts{}));
    EXPECT_EQ(fixture.coordinator->field_recompute_count(), 1U);
    const auto initial = fixture.coordinator->finish_turn();
    EXPECT_EQ(initial.lod.full, 9U);
    EXPECT_EQ(initial.lod.coarse, 16U);
    EXPECT_EQ(initial.lod.frozen, 0U);
    EXPECT_EQ(initial.loads, 25U);

    const auto batch = fixture.coordinator->advance_hours(240);
    EXPECT_EQ(batch.sites.size(), 9U);
    EXPECT_EQ(batch.site_hours_advanced, 9U * 240U);
    EXPECT_EQ(batch.site_xun_boundaries, 9U);
    EXPECT_EQ(batch.region_xun_advances, 1U);
    EXPECT_EQ(aetheria::world::turn_clock(fixture.region).now,
              aetheria::time::Tick{} + aetheria::time::kXun);
    std::cout << "streaming_field full=" << initial.lod.full
              << " coarse_outer=" << initial.lod.coarse
              << " batch_sites=" << batch.sites.size()
              << " region_xun_advances=" << batch.region_xun_advances << '\n';
}

TEST(SiteStreaming, BoundaryPacingLoadAndUnloadCountsStayBounded) {
    const auto ten = pace_boundary(10);
    const auto twenty = pace_boundary(20);
    EXPECT_EQ(ten.loads, 30U);
    EXPECT_EQ(ten.unloads, 0U);
    EXPECT_EQ(twenty.loads, ten.loads);
    EXPECT_EQ(twenty.unloads, ten.unloads);
    std::cout << "streaming_pacing buffered_crossings=10/20 loads=" << ten.loads << '/'
              << twenty.loads << " unloads=" << ten.unloads << '/' << twenty.unloads << '\n';
}

TEST(SiteStreaming, DemotionAndEvictionWaitForTurnEnd) {
    StreamingFixture fixture;
    fixture.coordinator->player_crossed_tile(kLeftCenter);
    static_cast<void>(fixture.coordinator->finish_turn());
    const auto before_move = fixture.coordinator->lod_counts();
    const auto loads_before = fixture.coordinator->load_count();

    fixture.coordinator->player_crossed_tile(kRightCenter);
    EXPECT_EQ(fixture.coordinator->lod_counts(), before_move);
    EXPECT_EQ(fixture.coordinator->load_count(), loads_before);
    EXPECT_EQ(fixture.coordinator->unload_count(), 0U);

    const auto move_tail = fixture.coordinator->finish_turn();
    EXPECT_EQ(move_tail.lod.full, 9U);
    EXPECT_EQ(move_tail.lod.coarse, 16U);
    EXPECT_EQ(move_tail.lod.frozen, 5U);
    EXPECT_EQ(fixture.coordinator->unload_count(), 0U);
    const auto frozen_key = aetheria::zone::child_key(kStreamingRegionKey, 2, 4);
    const auto frozen = fixture.manager.get(frozen_key);
    ASSERT_TRUE(frozen.has_value());
    ASSERT_TRUE(fixture.manager.with(*frozen, [](const aetheria::zone::Zone& site) {
        EXPECT_EQ(site.lod, aetheria::zone::LodLevel::Frozen);
        EXPECT_EQ(site.reg.view<const aetheria::site::SiteDigest>().size(), 1U);
        EXPECT_TRUE(site.reg.view<const aetheria::site::CityBuildState>().empty());
        EXPECT_FALSE(std::get<aetheria::zone::SitePayload>(site.payload)
                         .layers.procedural.valid_layout());
    }));

    const auto batch = fixture.coordinator->advance_hours(240);
    EXPECT_EQ(batch.region_xun_advances, 1U);
    EXPECT_EQ(fixture.coordinator->lod_counts().frozen, 5U);
    EXPECT_EQ(fixture.coordinator->unload_count(), 0U);

    const auto next_tail = fixture.coordinator->finish_turn();
    EXPECT_EQ(next_tail.unloads, 5U);
    EXPECT_EQ(next_tail.lod.frozen, 0U);
    EXPECT_EQ(fixture.coordinator->unload_count(), 5U);
}

TEST(SiteStreaming, FrozenWildernessDropsAndThawRebuildsProceduralEntities) {
    StreamingFixture fixture{aetheria::world::SettlementTier::None};
    fixture.coordinator->player_crossed_tile(kLeftCenter);
    static_cast<void>(fixture.coordinator->finish_turn());
    const auto key = aetheria::zone::child_key(kStreamingRegionKey, 2, 4);
    const auto handle = fixture.manager.get(key);
    ASSERT_TRUE(handle.has_value());
    ASSERT_TRUE(fixture.manager.with(*handle, [](const aetheria::zone::Zone& site) {
        EXPECT_FALSE(site.reg.view<const aetheria::site::SitePosition>().empty());
    }));

    fixture.coordinator->player_crossed_tile(kRightCenter);
    static_cast<void>(fixture.coordinator->finish_turn());
    ASSERT_TRUE(fixture.manager.with(*handle, [](const aetheria::zone::Zone& site) {
        EXPECT_EQ(site.lod, aetheria::zone::LodLevel::Frozen);
        EXPECT_TRUE(site.reg.view<const aetheria::site::SitePosition>().empty());
    }));

    fixture.coordinator->player_crossed_tile(kLeftCenter);
    static_cast<void>(fixture.coordinator->finish_turn());
    ASSERT_TRUE(fixture.manager.with(*handle, [](const aetheria::zone::Zone& site) {
        EXPECT_EQ(site.lod, aetheria::zone::LodLevel::Coarse);
        EXPECT_FALSE(site.reg.view<const aetheria::site::SitePosition>().empty());
    }));
}

}  // namespace

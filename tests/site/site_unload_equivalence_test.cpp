#include "core/site/site_build_loop.h"
#include "core/site/site_lifecycle.h"
#include "core/site/site_materialize.h"
#include "core/site/site_reduction.h"
#include "core/world/region_simulation.h"
#include "tests/site/site_reduction_test_support.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::site::CityBuilding;
using aetheria::site::PendingConstruction;
using aetheria::site::PersistentBuilding;
using aetheria::site::SiteCatchUpReport;
using aetheria::tests::kReductionCoordinate;
using aetheria::tests::kReductionRegionId;
using aetheria::tests::kReductionWorldSeed;
using aetheria::tests::reduction_region;
using aetheria::tests::test_ruleset;

constexpr auto kRegionKey =
    aetheria::zone::child_key(aetheria::zone::kRootZone, kReductionRegionId, 0);
constexpr auto kSiteKey = aetheria::zone::child_key(kRegionKey, 0, 0);

struct LifecycleFixture {
    aetheria::zone::Zone region{kRegionKey};
    aetheria::zone::Zone site{kSiteKey};
};

struct TileQuantities {
    std::uint32_t population{};
    std::uint16_t development{};
    std::uint64_t food{};
    std::uint64_t production{};
};

struct PathResult {
    TileQuantities quantities;
    std::vector<PersistentBuilding> objects;
    std::vector<CityBuilding> city_buildings;
    std::vector<PendingConstruction> pending;
    SiteCatchUpReport catch_up;
};

void initialize_lifecycle_fixture(
    LifecycleFixture& fixture,
    aetheria::world::FactionId owner = static_cast<aetheria::world::FactionId>(1)) {
    auto tiles = reduction_region();
    tiles.owner[0] = owner;
    fixture.site = aetheria::site::materialize_site_zone(
        tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, test_ruleset());
    const auto first_hash = aetheria::site::hash_site_skeleton(
        std::get<aetheria::zone::SitePayload>(fixture.site.payload).layers.procedural.skeleton);
    const auto rebuilt_hash = aetheria::site::hash_site_skeleton(aetheria::site::build_site_skeleton(
        aetheria::site::split_site_vars(tiles, kReductionCoordinate).slow,
        aetheria::site::derive_site_seed(kReductionWorldSeed, kReductionRegionId, 0, 0),
        test_ruleset()));
    if (first_hash != rebuilt_hash) {
        throw std::runtime_error{"生命週期 fixture 初次骨架不能確定重建"};
    }
    auto& region_tiles =
        std::get<aetheria::zone::RegionPayload>(fixture.region.payload)
            .layers.emplace(0, std::move(tiles))
            .first->second;
    const auto after_move_hash = aetheria::site::hash_site_skeleton(
        aetheria::site::build_site_skeleton(
            aetheria::site::split_site_vars(region_tiles, kReductionCoordinate).slow,
            aetheria::site::derive_site_seed(kReductionWorldSeed, kReductionRegionId, 0, 0),
            test_ruleset()));
    if (first_hash != after_move_hash) {
        throw std::runtime_error{"生命週期 fixture 移入 Region 後骨架輸入改變"};
    }
    const auto meta = *fixture.region.reg.view<aetheria::zone::ZoneMeta>().begin();
    fixture.region.reg.emplace<aetheria::world::TurnClock>(meta);
    aetheria::site::enter_full_site(fixture.site, region_tiles, kReductionCoordinate);
    auto& state = aetheria::site::city_build_state(fixture.site);
    state.buildings = {{"city.house", {10, 10}},
                       {"city.farm", {40, 40}},
                       {"city.square", {12, 10}},
                       {"city.workshop", {20, 20}},
                       {"city.mine", {23, 20}}};
    // 非相鄰的 square 會完成並證明 pending 補算真的執行，但不改經濟校準輸出。
    state.pending = {{"city.square", {30, 30}, 48}};
}

[[nodiscard]] aetheria::time::Tick region_now(const aetheria::zone::Zone& region) {
    const auto clocks = region.reg.view<const aetheria::world::TurnClock>();
    if (clocks.size() != 1U) {
        throw std::runtime_error{"測試 Region 缺少唯一時鐘"};
    }
    return clocks.get<const aetheria::world::TurnClock>(*clocks.begin()).now;
}

void advance_absent_to(aetheria::zone::Zone& region, aetheria::time::Tick target,
                       const aetheria::world::RegionTurnPipeline& pipeline) {
    while (region_now(region) < target) {
        pipeline.advance_xun(region);
    }
    ASSERT_EQ(region_now(region), target);
}

[[nodiscard]] TileQuantities quantities(const aetheria::world::RegionTiles& tiles) {
    return {
        tiles.reduction_value<aetheria::world::PopulationReduction>(kReductionCoordinate),
        tiles.reduction_value<aetheria::world::DevelopmentLevelReduction>(kReductionCoordinate),
        tiles.reduction_value<aetheria::world::FoodStockReduction>(kReductionCoordinate),
        tiles.reduction_value<aetheria::world::ProductionStockReduction>(kReductionCoordinate),
    };
}

[[nodiscard]] double relative_error(std::uint64_t a, std::uint64_t b) {
    const auto denominator = std::max<std::uint64_t>({a, b, 1U});
    return static_cast<double>(a > b ? a - b : b - a) /
           static_cast<double>(denominator);
}

[[nodiscard]] double maximum_error(const TileQuantities& a, const TileQuantities& b) {
    return std::max({relative_error(a.population, b.population),
                     relative_error(a.development, b.development),
                     relative_error(a.food, b.food), relative_error(a.production, b.production)});
}

[[nodiscard]] PathResult run_full(std::uint32_t xun) {
    LifecycleFixture fixture;
    initialize_lifecycle_fixture(fixture);
    aetheria::zone::InMemoryZoneStore store{test_ruleset()};
    aetheria::site::SiteTurnPipeline pipeline{test_ruleset(), store};
    const auto elapsed = aetheria::time::kXun * static_cast<std::int64_t>(xun);
    const auto hours = static_cast<std::uint32_t>(elapsed / aetheria::time::kHour);
    static_cast<void>(pipeline.advance_hours(fixture.site, fixture.region, 0,
                                             kReductionCoordinate, hours));
    EXPECT_EQ(region_now(fixture.region), aetheria::time::Tick{} + elapsed);
    const auto& tiles =
        std::get<aetheria::zone::RegionPayload>(fixture.region.payload).layers.at(0);
    const auto& layers = std::get<aetheria::zone::SitePayload>(fixture.site.payload).layers;
    const auto& state = aetheria::site::city_build_state(fixture.site);
    return {quantities(tiles), layers.persistent.buildings, state.buildings, state.pending, {}};
}

[[nodiscard]] PathResult run_absent(std::uint32_t xun) {
    LifecycleFixture fixture;
    initialize_lifecycle_fixture(fixture);
    auto& tiles = std::get<aetheria::zone::RegionPayload>(fixture.region.payload).layers.at(0);
    const auto slow_before = aetheria::site::split_site_vars(tiles, kReductionCoordinate).slow;
    const auto before_unload_hash = aetheria::site::hash_site_skeleton(
        aetheria::site::build_site_skeleton(
            slow_before,
            aetheria::site::derive_site_seed(kReductionWorldSeed, kReductionRegionId, 0, 0),
            test_ruleset()));
    EXPECT_EQ(before_unload_hash,
              aetheria::site::hash_site_skeleton(
                  std::get<aetheria::zone::SitePayload>(fixture.site.payload)
                      .layers.procedural.skeleton));
    aetheria::zone::InMemoryZoneStore store{test_ruleset()};
    store.save(fixture.site);
    tiles.site[0].has_live_site = false;
    tiles.site[0].lod = aetheria::zone::LodLevel::Absent;
    aetheria::zone::ZoneManager manager{store};
    const auto handle = aetheria::site::rematerialize_site_zone(
        manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
        test_ruleset());
    const bool entered = manager.with(handle, [&](aetheria::zone::Zone& site) {
        aetheria::site::enter_full_site(site, tiles, kReductionCoordinate);
    });
    if (!entered) {
        throw std::runtime_error{"卸載前 Site 未留在 manager"};
    }
    aetheria::site::unload_site_zone(manager, handle, tiles, kReductionCoordinate,
                                     kReductionWorldSeed, kReductionRegionId,
                                     aetheria::time::Tick{});
    EXPECT_FALSE(manager.get(kSiteKey).has_value());

    aetheria::world::RegionTurnPipeline region_pipeline{test_ruleset(), store};
    const auto elapsed = aetheria::time::kXun * static_cast<std::int64_t>(xun);
    const auto target = aetheria::time::Tick{} + elapsed;
    advance_absent_to(fixture.region, target, region_pipeline);
    const auto slow_after = aetheria::site::split_site_vars(tiles, kReductionCoordinate).slow;
    EXPECT_EQ(slow_before, slow_after);
    const auto before_reload_hash = aetheria::site::hash_site_skeleton(
        aetheria::site::build_site_skeleton(
            aetheria::site::split_site_vars(tiles, kReductionCoordinate).slow,
            aetheria::site::derive_site_seed(kReductionWorldSeed, kReductionRegionId, 0, 0),
            test_ruleset()));
    EXPECT_EQ(before_unload_hash, before_reload_hash);
    SiteCatchUpReport report;
    const auto reloaded = aetheria::site::rematerialize_site_zone(
        manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, target,
        test_ruleset(), &report);
    PathResult result;
    const bool borrowed = manager.with(reloaded, [&](const aetheria::zone::Zone& site) {
        aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, site);
        const auto& layers = std::get<aetheria::zone::SitePayload>(site.payload).layers;
        const auto& state = aetheria::site::city_build_state(site);
        result = {quantities(tiles), layers.persistent.buildings, state.buildings, state.pending,
                  report};
    });
    if (!borrowed) {
        throw std::runtime_error{"重載 Site 未留在 manager"};
    }
    return result;
}

TEST(SiteLifecycle, FullAndAbsentClockPathsStayWithinTenPercentAndKeepPersistentObjects) {
    constexpr std::array<std::uint32_t, 4> samples{1, 5, 20, 100};
    for (const auto xun : samples) {
        const auto full = run_full(xun);
        const auto absent = run_absent(xun);
        const auto error = maximum_error(full.quantities, absent.quantities);
        EXPECT_LT(error, 0.10) << "N=" << xun;
        EXPECT_EQ(full.objects, absent.objects) << "N=" << xun;
        EXPECT_EQ(full.city_buildings, absent.city_buildings) << "N=" << xun;
        EXPECT_EQ(full.pending, absent.pending) << "N=" << xun;
        EXPECT_EQ(absent.catch_up.pending_advanced, 1U);
        EXPECT_EQ(absent.catch_up.constructions_completed, 1U);
        std::cout << "site_unload_equivalence N=" << xun
                  << " full(pop/dev/food/prod)=" << full.quantities.population << '/'
                  << full.quantities.development << '/' << full.quantities.food << '/'
                  << full.quantities.production << " absent(pop/dev/food/prod)="
                  << absent.quantities.population << '/' << absent.quantities.development << '/'
                  << absent.quantities.food << '/' << absent.quantities.production
                  << " max_relative_error=" << error << " pending_advanced="
                  << absent.catch_up.pending_advanced << " completed="
                  << absent.catch_up.constructions_completed << '\n';
    }
}

TEST(SiteLifecycle, SkippingCatchUpIsDetectedAboveTenPercent) {
    constexpr std::uint32_t xun = 20;
    const auto full = run_full(xun);
    LifecycleFixture fixture;
    initialize_lifecycle_fixture(fixture);
    auto& tiles = std::get<aetheria::zone::RegionPayload>(fixture.region.payload).layers.at(0);
    aetheria::zone::InMemoryZoneStore store{test_ruleset()};
    store.save(fixture.site);
    tiles.site[0].has_live_site = false;
    tiles.site[0].lod = aetheria::zone::LodLevel::Absent;
    aetheria::zone::ZoneManager manager{store};
    const auto handle = aetheria::site::rematerialize_site_zone(
        manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
        test_ruleset());
    ASSERT_TRUE(manager.with(handle, [&](aetheria::zone::Zone& site) {
        aetheria::site::enter_full_site(site, tiles, kReductionCoordinate);
    }));
    aetheria::site::unload_site_zone(manager, handle, tiles, kReductionCoordinate,
                                     kReductionWorldSeed, kReductionRegionId,
                                     aetheria::time::Tick{});
    aetheria::world::RegionTurnPipeline region_pipeline{test_ruleset(), store};
    const auto target = aetheria::time::Tick{} +
                        aetheria::time::kXun * static_cast<std::int64_t>(xun);
    advance_absent_to(fixture.region, target, region_pipeline);

    ASSERT_TRUE(manager.load(kSiteKey));
    const auto cold = *manager.get(kSiteKey);
    TileQuantities broken;
    ASSERT_TRUE(manager.with(cold, [&](aetheria::zone::Zone& site) {
        ASSERT_EQ(site.reg.view<const aetheria::site::SiteDigest>().size(), 1U);
        ASSERT_TRUE(site.reg.view<const aetheria::site::CityBuildState>().empty());
        site.lod = aetheria::zone::LodLevel::Coarse;
        tiles.site[0].has_live_site = true;
        aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, site);
        broken = quantities(tiles);
    }));
    const auto error = maximum_error(full.quantities, broken);
    EXPECT_GT(error, 0.10);
    std::cout << "site_unload_negative_control N=20 full_population="
              << full.quantities.population << " skipped_catch_up_population="
              << broken.population << " max_relative_error=" << error << '\n';
}

TEST(SiteLifecycle, AgingIsClosedFormAndSaturatesAfterTwentyFourXun) {
    LifecycleFixture fixture;
    initialize_lifecycle_fixture(fixture, aetheria::world::FactionId{});
    auto& tiles = std::get<aetheria::zone::RegionPayload>(fixture.region.payload).layers.at(0);
    aetheria::zone::InMemoryZoneStore store{test_ruleset()};
    store.save(fixture.site);
    tiles.site[0].has_live_site = false;
    tiles.site[0].lod = aetheria::zone::LodLevel::Absent;
    aetheria::zone::ZoneManager manager{store};
    const auto handle = aetheria::site::rematerialize_site_zone(
        manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
        test_ruleset());
    ASSERT_TRUE(manager.with(handle, [&](aetheria::zone::Zone& site) {
        aetheria::site::enter_full_site(site, tiles, kReductionCoordinate);
    }));
    aetheria::site::unload_site_zone(manager, handle, tiles, kReductionCoordinate,
                                     kReductionWorldSeed, kReductionRegionId,
                                     aetheria::time::Tick{});
    SiteCatchUpReport report;
    constexpr std::uint32_t xun = 10'000;
    const auto now = aetheria::time::Tick{} +
                     aetheria::time::kXun * static_cast<std::int64_t>(xun);
    const auto reloaded = aetheria::site::rematerialize_site_zone(
        manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId, now,
        test_ruleset(), &report);
    ASSERT_TRUE(manager.with(reloaded, [](const aetheria::zone::Zone& site) {
        const auto& objects =
            std::get<aetheria::zone::SitePayload>(site.payload).layers.persistent.buildings;
        ASSERT_EQ(objects.size(), 1U);
        EXPECT_EQ(objects.front().state, aetheria::site::BuildingState::Ruined);
        EXPECT_EQ(objects.front().aging_seconds,
                  static_cast<std::uint32_t>(aetheria::site::kBuildingAgingCap));
    }));
    EXPECT_TRUE(report.aging_cap_hit);
    EXPECT_EQ(report.persistent_objects_advanced, 1U);
    EXPECT_EQ(report.aging_transitions, 3U);
    EXPECT_EQ(report.aging_seconds_applied,
              static_cast<std::uint64_t>(aetheria::site::kBuildingAgingCap));
    std::cout << "site_unload_saturation N=" << xun
              << " objects_advanced=" << report.persistent_objects_advanced
              << " aging_transitions=" << report.aging_transitions
              << " cap_hit=" << report.aging_cap_hit << " cap_xun=24 first_clamped_N=25\n";
}

}  // namespace

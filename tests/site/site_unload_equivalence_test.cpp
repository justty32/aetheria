#include "core/site/site_build_loop.h"
#include "core/site/site_lifecycle.h"
#include "core/site/site_materialize.h"
#include "core/site/site_reduction.h"
#include "core/world/region_simulation.h"
#include "core/worldgen/region_seed.h"
#include "tests/site/site_reduction_test_support.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>
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

struct SequenceResult {
    TileQuantities quantities;
    std::uint32_t live_xun{};
    std::uint32_t absent_xun{};
    std::uint32_t transitions{};
};

[[nodiscard]] SequenceResult run_random_sequence(std::uint64_t random_bits,
                                                 std::uint32_t xun_count) {
    LifecycleFixture fixture;
    initialize_lifecycle_fixture(fixture);
    auto& tiles = std::get<aetheria::zone::RegionPayload>(fixture.region.payload).layers.at(0);
    aetheria::zone::InMemoryZoneStore store{test_ruleset()};
    store.save(fixture.site);
    tiles.site[0].has_live_site = false;
    tiles.site[0].lod = aetheria::zone::LodLevel::Absent;
    aetheria::zone::ZoneManager manager{store};
    auto handle = aetheria::site::rematerialize_site_zone(
        manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
        test_ruleset());
    if (!manager.with(handle, [&](aetheria::zone::Zone& site) {
            aetheria::site::enter_full_site(site, tiles, kReductionCoordinate);
        })) {
        throw std::runtime_error{"隨機序列無法建立初始 L_FULL Site"};
    }
    aetheria::site::SiteTurnPipeline site_pipeline{test_ruleset(), store};
    aetheria::world::RegionTurnPipeline region_pipeline{test_ruleset(), store};
    bool live = true;
    SequenceResult result;
    for (std::uint32_t xun = 0; xun < xun_count; ++xun) {
        const bool next_live = ((random_bits >> xun) & UINT64_C(1)) != 0;
        if (next_live != live) {
            ++result.transitions;
        }
        if (next_live) {
            if (!live) {
                handle = aetheria::site::rematerialize_site_zone(
                    manager, tiles, kReductionCoordinate, kReductionWorldSeed,
                    kReductionRegionId, region_now(fixture.region), test_ruleset());
                if (!manager.with(handle, [&](aetheria::zone::Zone& site) {
                        aetheria::site::enter_full_site(site, tiles, kReductionCoordinate);
                    })) {
                    throw std::runtime_error{"隨機序列重載後無法進入 L_FULL"};
                }
                live = true;
            }
            if (!manager.with(handle, [&](aetheria::zone::Zone& site) {
                    static_cast<void>(site_pipeline.advance_hours(
                        site, fixture.region, 0, kReductionCoordinate, 240));
                })) {
                throw std::runtime_error{"隨機序列找不到 live Site"};
            }
            ++result.live_xun;
        } else {
            if (live) {
                aetheria::site::unload_site_zone(
                    manager, handle, tiles, kReductionCoordinate, kReductionWorldSeed,
                    kReductionRegionId, region_now(fixture.region));
                live = false;
            }
            region_pipeline.advance_xun(fixture.region);
            ++result.absent_xun;
        }
    }
    if (!live) {
        handle = aetheria::site::rematerialize_site_zone(
            manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
            region_now(fixture.region), test_ruleset());
        if (!manager.with(handle, [&](aetheria::zone::Zone& site) {
                aetheria::site::enter_full_site(site, tiles, kReductionCoordinate);
            })) {
            throw std::runtime_error{"隨機序列終點重載失敗"};
        }
    }
    if (!manager.with(handle, [&](const aetheria::zone::Zone& site) {
            aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, site);
        })) {
        throw std::runtime_error{"隨機序列終點歸約失敗"};
    }
    result.quantities = quantities(tiles);
    return result;
}

struct ControlledUnloadResult {
    TileQuantities quantities;
    std::uint32_t unloads{};
    std::uint32_t live_xun{};
    std::uint32_t absent_xun{};
};

[[nodiscard]] ControlledUnloadResult run_controlled_unloads(std::uint32_t unload_count) {
    constexpr std::uint32_t total_xun = 20;
    constexpr std::uint32_t controlled_live_xun = 10;
    constexpr std::uint32_t controlled_absent_xun = 10;
    if (unload_count == 0) {
        return {run_full(total_xun).quantities, 0, total_xun, 0};
    }
    if (controlled_absent_xun % std::min(unload_count, controlled_absent_xun) != 0) {
        throw std::invalid_argument{"診斷卸載次數不能整分固定的離線旬數"};
    }

    LifecycleFixture fixture;
    initialize_lifecycle_fixture(fixture);
    auto& tiles = std::get<aetheria::zone::RegionPayload>(fixture.region.payload).layers.at(0);
    aetheria::zone::InMemoryZoneStore store{test_ruleset()};
    store.save(fixture.site);
    tiles.site[0].has_live_site = false;
    tiles.site[0].lod = aetheria::zone::LodLevel::Absent;
    aetheria::zone::ZoneManager manager{store};
    auto handle = aetheria::site::rematerialize_site_zone(
        manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
        test_ruleset());
    if (!manager.with(handle, [&](aetheria::zone::Zone& site) {
            aetheria::site::enter_full_site(site, tiles, kReductionCoordinate);
        })) {
        throw std::runtime_error{"控制卸載診斷無法建立 L_FULL Site"};
    }
    aetheria::site::SiteTurnPipeline site_pipeline{test_ruleset(), store};
    aetheria::world::RegionTurnPipeline region_pipeline{test_ruleset(), store};
    ControlledUnloadResult result;
    const auto productive_unloads = std::min(unload_count, controlled_absent_xun);
    const auto live_chunk = controlled_live_xun / productive_unloads;
    const auto absent_chunk = controlled_absent_xun / productive_unloads;
    const auto zero_time_unloads = unload_count - productive_unloads;

    const auto unload_and_reload = [&](std::uint32_t absent_xun) {
        aetheria::site::unload_site_zone(
            manager, handle, tiles, kReductionCoordinate, kReductionWorldSeed,
            kReductionRegionId, region_now(fixture.region));
        ++result.unloads;
        for (std::uint32_t xun = 0; xun < absent_xun; ++xun) {
            region_pipeline.advance_xun(fixture.region);
            ++result.absent_xun;
        }
        handle = aetheria::site::rematerialize_site_zone(
            manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
            region_now(fixture.region), test_ruleset());
        if (!manager.with(handle, [&](aetheria::zone::Zone& site) {
                aetheria::site::enter_full_site(site, tiles, kReductionCoordinate);
            })) {
            throw std::runtime_error{"控制卸載診斷重載失敗"};
        }
    };

    for (std::uint32_t episode = 0; episode < productive_unloads; ++episode) {
        if (!manager.with(handle, [&](aetheria::zone::Zone& site) {
                static_cast<void>(site_pipeline.advance_hours(
                    site, fixture.region, 0, kReductionCoordinate, live_chunk * 240U));
            })) {
            throw std::runtime_error{"控制卸載診斷找不到 live Site"};
        }
        result.live_xun += live_chunk;
        unload_and_reload(absent_chunk);
        if (episode < zero_time_unloads) {
            unload_and_reload(0);
        }
    }
    if (!manager.with(handle, [&](const aetheria::zone::Zone& site) {
            aetheria::site::reduce_live_site_xun(tiles, kReductionCoordinate, site);
        })) {
        throw std::runtime_error{"控制卸載診斷終點歸約失敗"};
    }
    if (result.unloads != unload_count || result.live_xun + result.absent_xun != total_xun) {
        throw std::runtime_error{"控制卸載診斷沒有守住卸載次數或總旬數"};
    }
    result.quantities = quantities(tiles);
    return result;
}

struct Distribution {
    double mean{};
    double standard_deviation{};
};

[[nodiscard]] bool systematically_high(double mean, double baseline) noexcept {
    return mean > baseline;
}

template <typename Value>
[[nodiscard]] Distribution distribution(const std::vector<TileQuantities>& samples,
                                        Value TileQuantities::*member) {
    double sum{};
    for (const auto& sample : samples) {
        sum += static_cast<double>(sample.*member);
    }
    const auto mean = sum / static_cast<double>(samples.size());
    double squared_difference{};
    for (const auto& sample : samples) {
        const auto difference = static_cast<double>(sample.*member) - mean;
        squared_difference += difference * difference;
    }
    return {mean, std::sqrt(squared_difference / static_cast<double>(samples.size()))};
}

void report_unbias_row(std::string_view name, double baseline,
                       const Distribution& observed) {
    const auto bias = (observed.mean - baseline) / std::max(1.0, baseline);
    const auto direction = bias > 0.0 ? "high" : bias < 0.0 ? "low" : "neutral";
    std::cout << "site_unbias_" << name << " baseline=" << baseline
              << " mean=" << observed.mean << " stddev=" << observed.standard_deviation
              << " bias=" << bias << " direction=" << direction << '\n';
    EXPECT_LT(std::abs(bias), 0.05) << name;
    EXPECT_FALSE(systematically_high(observed.mean, baseline))
        << name << " 反覆進出造成系統性偏高";
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

TEST(SiteLifecycle, HundredRandomLoadUnloadSequencesStayBoundedAndCannotBiasHigh) {
    constexpr std::size_t sample_count = 100;
    constexpr std::uint32_t xun_count = 20;
    const auto baseline = run_full(xun_count).quantities;
    std::vector<TileQuantities> samples;
    samples.reserve(sample_count);
    std::uint32_t live_xun{};
    std::uint32_t absent_xun{};
    std::uint32_t transitions{};
    for (std::size_t sample = 0; sample < sample_count; ++sample) {
        auto bits = aetheria::worldgen::splitmix64(UINT64_C(0x4D410001) ^ sample);
        bits |= UINT64_C(1);
        bits &= ~UINT64_C(2);
        const auto result = run_random_sequence(bits, xun_count);
        samples.push_back(result.quantities);
        live_xun += result.live_xun;
        absent_xun += result.absent_xun;
        transitions += result.transitions;
    }
    ASSERT_EQ(samples.size(), sample_count);
    EXPECT_GT(live_xun, 0U);
    EXPECT_GT(absent_xun, 0U);
    EXPECT_GE(transitions, sample_count);

    const auto population = distribution(samples, &TileQuantities::population);
    const auto development = distribution(samples, &TileQuantities::development);
    const auto food = distribution(samples, &TileQuantities::food);
    const auto production = distribution(samples, &TileQuantities::production);
    report_unbias_row("population", baseline.population, population);
    report_unbias_row("development", baseline.development, development);
    report_unbias_row("food", static_cast<double>(baseline.food), food);
    report_unbias_row("production", static_cast<double>(baseline.production), production);

    EXPECT_TRUE(systematically_high(static_cast<double>(baseline.population) * 1.01,
                                    baseline.population));
    std::cout << "site_unbias_sequences count=" << sample_count << " xun=" << xun_count
              << " live_xun=" << live_xun << " absent_xun=" << absent_xun
              << " transitions=" << transitions
              << " positive_control_direction=high positive_control_bias=0.01\n";
}

TEST(SiteLifecycle, ReportsBiasByUnloadCountAtFixedTwentyXun) {
    constexpr std::array<std::uint32_t, 5> unload_counts{0, 2, 5, 10, 20};
    const auto baseline = run_controlled_unloads(0);
    for (const auto unloads : unload_counts) {
        const auto observed = run_controlled_unloads(unloads);
        const auto bias = [](std::uint64_t value, std::uint64_t reference) {
            return (static_cast<double>(value) - static_cast<double>(reference)) /
                   std::max(1.0, static_cast<double>(reference));
        };
        EXPECT_EQ(observed.unloads, unloads);
        EXPECT_EQ(observed.live_xun + observed.absent_xun, 20U);
        std::cout << "site_unload_count_bias unloads=" << unloads
                  << " total_xun=20 live_xun=" << observed.live_xun
                  << " absent_xun=" << observed.absent_xun
                  << " population_bias="
                  << bias(observed.quantities.population, baseline.quantities.population)
                  << " development_bias="
                  << bias(observed.quantities.development, baseline.quantities.development)
                  << " food_bias=" << bias(observed.quantities.food, baseline.quantities.food)
                  << " production_bias="
                  << bias(observed.quantities.production, baseline.quantities.production)
                  << '\n';
    }
}

TEST(SiteLifecycle, PopulationFractionSurvivesUnloadReload) {
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
    aetheria::site::SiteTurnPipeline pipeline{test_ruleset(), store};
    std::int64_t before{};
    ASSERT_TRUE(manager.with(handle, [&](aetheria::zone::Zone& site) {
        static_cast<void>(pipeline.advance_hours(
            site, fixture.region, 0, kReductionCoordinate, 1));
        before = aetheria::site::city_build_state(site).economy.population_micro_remainder;
    }));
    ASSERT_NE(before, 0);
    aetheria::site::unload_site_zone(
        manager, handle, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
        region_now(fixture.region));
    const auto reloaded = aetheria::site::rematerialize_site_zone(
        manager, tiles, kReductionCoordinate, kReductionWorldSeed, kReductionRegionId,
        region_now(fixture.region), test_ruleset());
    ASSERT_TRUE(manager.with(reloaded, [&](const aetheria::zone::Zone& site) {
        EXPECT_EQ(aetheria::site::city_build_state(site).economy.population_micro_remainder,
                  before);
    }));
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

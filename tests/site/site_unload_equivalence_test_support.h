#pragma once

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

namespace aetheria::tests::site_unload_equivalence {

using aetheria::site::CityBuilding;
using aetheria::site::PendingConstruction;
using aetheria::site::PersistentBuilding;
using aetheria::site::SiteCatchUpReport;
using aetheria::tests::kReductionCoordinate;
using aetheria::tests::kReductionRegionId;
using aetheria::tests::kReductionWorldSeed;
using aetheria::tests::reduction_region;
using aetheria::tests::test_ruleset;

inline constexpr auto kRegionKey =
    aetheria::zone::child_key(aetheria::zone::kRootZone, kReductionRegionId, 0);
inline constexpr auto kSiteKey = aetheria::zone::child_key(kRegionKey, 0, 0);

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

inline void initialize_lifecycle_fixture(
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

[[nodiscard]] inline aetheria::time::Tick region_now(const aetheria::zone::Zone& region) {
    const auto clocks = region.reg.view<const aetheria::world::TurnClock>();
    if (clocks.size() != 1U) {
        throw std::runtime_error{"測試 Region 缺少唯一時鐘"};
    }
    return clocks.get<const aetheria::world::TurnClock>(*clocks.begin()).now;
}

inline void advance_absent_to(aetheria::zone::Zone& region, aetheria::time::Tick target,
                       const aetheria::world::RegionTurnPipeline& pipeline) {
    while (region_now(region) < target) {
        pipeline.advance_xun(region);
    }
    ASSERT_EQ(region_now(region), target);
}

[[nodiscard]] inline TileQuantities quantities(const aetheria::world::RegionTiles& tiles) {
    return {
        tiles.reduction_value<aetheria::world::PopulationReduction>(kReductionCoordinate),
        tiles.reduction_value<aetheria::world::DevelopmentLevelReduction>(kReductionCoordinate),
        tiles.reduction_value<aetheria::world::FoodStockReduction>(kReductionCoordinate),
        tiles.reduction_value<aetheria::world::ProductionStockReduction>(kReductionCoordinate),
    };
}

[[nodiscard]] inline double relative_error(std::uint64_t a, std::uint64_t b) {
    const auto denominator = std::max<std::uint64_t>({a, b, 1U});
    return static_cast<double>(a > b ? a - b : b - a) /
           static_cast<double>(denominator);
}

[[nodiscard]] inline double maximum_error(const TileQuantities& a, const TileQuantities& b) {
    return std::max({relative_error(a.population, b.population),
                     relative_error(a.development, b.development),
                     relative_error(a.food, b.food), relative_error(a.production, b.production)});
}

[[nodiscard]] inline PathResult run_full(std::uint32_t xun) {
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

}  // namespace aetheria::tests::site_unload_equivalence

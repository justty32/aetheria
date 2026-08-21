#include "tests/site/site_fill_test_support.h"

#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::site::SiteEdgeRef;
using aetheria::site::SiteFastVars;
using aetheria::site::SiteZoning;
using aetheria::tests::fill_counts;
using aetheria::tests::fill_slow_vars;
using aetheria::tests::fill_tile_index;
using aetheria::tests::frontage_is_road;
using aetheria::tests::generate_fill;
using aetheria::tests::test_ruleset;

TEST(SiteFill, FastVariablesChangeFillHashButNeverSkeletonHash) {
    auto baseline_fast = SiteFastVars{};
    baseline_fast.settlement = aetheria::world::SettlementTier::Town;
    baseline_fast.population = 250;
    baseline_fast.development_level = 1;
    auto population_fast = baseline_fast;
    population_fast.population = 2000;
    auto development_fast = baseline_fast;
    development_fast.development_level = 10;
    auto owner_fast = baseline_fast;
    owner_fast.owner = static_cast<aetheria::world::FactionId>(1);
    auto defense_fast = baseline_fast;
    defense_fast.defense = 50;
    auto damage_fast = defense_fast;
    damage_fast.damage = 30;

    const auto baseline = generate_fill(baseline_fast);
    const auto population = generate_fill(population_fast);
    const auto development = generate_fill(development_fast);
    const auto owner = generate_fill(owner_fast);
    const auto defense = generate_fill(defense_fast);
    const auto damage = generate_fill(damage_fast);
    const auto baseline_s = aetheria::site::hash_site_skeleton(baseline.skeleton);
    const auto population_s = aetheria::site::hash_site_skeleton(population.skeleton);
    const auto development_s = aetheria::site::hash_site_skeleton(development.skeleton);
    const auto owner_s = aetheria::site::hash_site_skeleton(owner.skeleton);
    const auto defense_s = aetheria::site::hash_site_skeleton(defense.skeleton);
    const auto damage_s = aetheria::site::hash_site_skeleton(damage.skeleton);
    const auto baseline_f = aetheria::site::hash_site_fill(baseline);
    const auto population_f = aetheria::site::hash_site_fill(population);
    const auto development_f = aetheria::site::hash_site_fill(development);
    const auto owner_f = aetheria::site::hash_site_fill(owner);
    const auto defense_f = aetheria::site::hash_site_fill(defense);
    const auto damage_f = aetheria::site::hash_site_fill(damage);

    EXPECT_EQ(baseline_s, population_s);
    EXPECT_EQ(baseline_s, development_s);
    EXPECT_EQ(baseline_s, owner_s);
    EXPECT_EQ(baseline_s, defense_s);
    EXPECT_EQ(baseline_s, damage_s);
    EXPECT_NE(baseline_f, population_f);
    EXPECT_NE(baseline_f, development_f);
    EXPECT_NE(baseline_f, owner_f);
    EXPECT_NE(baseline_f, defense_f);
    EXPECT_NE(defense_f, damage_f);
    std::cout << "site_fill_skeleton_hashes baseline=" << baseline_s
              << " population=" << population_s << " development=" << development_s << '\n'
              << "site_fill_extended_skeleton_hashes owner=" << owner_s << " defense=" << defense_s
              << " damage=" << damage_s << '\n'
              << "site_fill_output_hashes baseline=" << baseline_f << " population=" << population_f
              << " development=" << development_f << " owner=" << owner_f
              << " defense=" << defense_f << " damage=" << damage_f << '\n';
}

[[nodiscard]] const aetheria::rules::EdgeDef&
edge_at(const aetheria::site::SiteProceduralLayer& layer, SiteEdgeRef edge) {
    const auto index =
        fill_tile_index(edge.tile.x, edge.tile.y) * 4U + static_cast<std::size_t>(edge.side);
    return *test_ruleset().edge(layer.edges.at(index));
}

TEST(SiteFill, ZeroTrunkRoadsSuppressWallsSoEveryWallHasAGate) {
    SiteFastVars fast{};
    fast.settlement = aetheria::world::SettlementTier::City;
    fast.population = 8000;
    fast.development_level = 20;
    fast.defense = 100;
    auto slow = fill_slow_vars();
    std::ranges::fill(slow.edges, *test_ruleset().find_edge("edge.none"));

    const auto layer = generate_fill(fast, slow);
    const auto wall_edge_count = std::ranges::count_if(layer.edges, [](auto edge) {
        return (test_ruleset().edge(edge)->flags & aetheria::rules::kEdgeWallFlag) != 0;
    });
    EXPECT_TRUE(layer.skeleton.gates.empty());
    EXPECT_EQ(layer.wall_ring_count, 0U);
    EXPECT_TRUE(layer.wall_gates.empty());
    EXPECT_EQ(wall_edge_count, 0U);
    std::cout << "site_zero_trunk_roads=0 wall_rings="
              << static_cast<unsigned>(layer.wall_ring_count) << " wall_edges=" << wall_edge_count
              << " wall_gates=" << layer.wall_gates.size() << '\n';
}

TEST(SiteFill, DefenseZeroHasNoWallAndHighDefenseHasTwoGatedRings) {
    SiteFastVars no_defense{};
    no_defense.settlement = aetheria::world::SettlementTier::City;
    no_defense.population = 8000;
    no_defense.development_level = 20;
    auto high_defense = no_defense;
    high_defense.defense = 100;

    const auto none = generate_fill(no_defense, fill_slow_vars(true));
    const auto high = generate_fill(high_defense, fill_slow_vars(true));
    EXPECT_EQ(none.wall_ring_count, 0U);
    EXPECT_TRUE(none.wall_gates.empty());
    ASSERT_EQ(high.skeleton.gates.size(), 4U);
    EXPECT_EQ(high.wall_ring_count, 2U);
    EXPECT_EQ(high.wall_gates.size(), high.skeleton.gates.size() * high.wall_ring_count);
    for (const auto gate : high.wall_gates) {
        const auto flags = edge_at(high, gate).flags;
        EXPECT_NE(flags & aetheria::rules::kEdgeWallFlag, 0U);
        EXPECT_NE(flags & aetheria::rules::kEdgeGateFlag, 0U);
        EXPECT_NE(flags & aetheria::rules::kEdgeOpenableFlag, 0U);
    }
}

TEST(SiteFill, EveryRoadSideGetsOneGatePerRingAcrossSeedsAndRoadMasks) {
    SiteFastVars fast{};
    fast.settlement = aetheria::world::SettlementTier::City;
    fast.population = 8000;
    fast.development_level = 20;
    fast.defense = 100;
    const auto none = *test_ruleset().find_edge("edge.none");
    const auto road = *test_ruleset().find_edge("edge.road");

    for (std::uint8_t mask = 0; mask < 16U; ++mask) {
        auto slow = fill_slow_vars();
        for (std::size_t side = 0; side < slow.edges.size(); ++side) {
            slow.edges[side] = (mask & (1U << side)) != 0 ? road : none;
        }
        for (std::uint64_t seed = 0; seed < 16U; ++seed) {
            const auto layer = aetheria::site::populate(
                aetheria::site::build_site_skeleton(slow, seed, test_ruleset()), fast,
                test_ruleset());
            const auto road_count = std::popcount(mask);
            if (road_count == 0) {
                EXPECT_EQ(layer.wall_ring_count, 0U);
                EXPECT_TRUE(layer.wall_gates.empty());
            } else {
                EXPECT_EQ(layer.wall_ring_count, 2U);
                EXPECT_EQ(layer.wall_gates.size(), road_count * layer.wall_ring_count);
            }
        }
    }
}

TEST(SiteFill, LandmarkComesFromOwnerStyleAndUsesNearestCommercialBlock) {
    SiteFastVars fast{};
    fast.owner = static_cast<aetheria::world::FactionId>(1);
    fast.settlement = aetheria::world::SettlementTier::City;
    fast.population = 8000;
    fast.development_level = 20;
    const auto layer = generate_fill(fast);

    std::vector<const aetheria::site::ProceduralBuilding*> landmarks;
    for (const auto& building : layer.buildings) {
        if (test_ruleset().building(building.def)->landmark) {
            landmarks.push_back(&building);
        }
    }
    ASSERT_EQ(landmarks.size(), 1U);
    const auto& style = *std::ranges::find(test_ruleset().site_fill_rules().faction_styles, 1U,
                                           &aetheria::rules::FactionLandmarkStyle::faction);
    EXPECT_NE(std::ranges::find(style.landmarks, landmarks.front()->def), style.landmarks.end());

    auto landmark_distance = std::numeric_limits<std::uint32_t>::max();
    auto nearest_commercial = std::numeric_limits<std::uint32_t>::max();
    for (std::size_t index = 0; index < layer.skeleton.blocks.size(); ++index) {
        if (layer.block_zoning[index] != SiteZoning::Commercial) {
            continue;
        }
        const auto& block = layer.skeleton.blocks[index];
        const auto center_x = static_cast<std::int32_t>(block.origin.x) + block.width / 2;
        const auto center_y = static_cast<std::int32_t>(block.origin.y) + block.height / 2;
        const auto distance = static_cast<std::uint32_t>(
            std::abs(center_x - static_cast<std::int32_t>(layer.skeleton.city_center.x)) +
            std::abs(center_y - static_cast<std::int32_t>(layer.skeleton.city_center.y)));
        nearest_commercial = std::min(nearest_commercial, distance);
        const auto& landmark = *landmarks.front();
        if (landmark.origin.x >= block.origin.x && landmark.origin.y >= block.origin.y &&
            landmark.origin.x + landmark.width <= block.origin.x + block.width &&
            landmark.origin.y + landmark.height <= block.origin.y + block.height) {
            landmark_distance = distance;
        }
    }
    EXPECT_EQ(landmark_distance, nearest_commercial);
}

TEST(SiteFill, DamagedBuildingsAreMeasurablyCloserToGatesAndBreaches) {
    SiteFastVars fast{};
    fast.owner = static_cast<aetheria::world::FactionId>(2);
    fast.settlement = aetheria::world::SettlementTier::City;
    fast.population = 8000;
    fast.development_level = 20;
    fast.defense = 60;
    fast.damage = 25;
    const auto layer = generate_fill(fast, fill_slow_vars(true));
    std::vector<SiteEdgeRef> sources = layer.wall_gates;
    sources.insert(sources.end(), layer.wall_breaches.begin(), layer.wall_breaches.end());
    ASSERT_FALSE(sources.empty());

    std::uint64_t all_distance{};
    std::uint64_t damaged_distance{};
    std::size_t damaged_count{};
    for (const auto& building : layer.buildings) {
        const aetheria::site::SiteXY center{
            static_cast<std::uint16_t>(building.origin.x + building.width / 2U),
            static_cast<std::uint16_t>(building.origin.y + building.height / 2U)};
        auto nearest = std::numeric_limits<std::uint32_t>::max();
        for (const auto source : sources) {
            nearest = std::min<std::uint32_t>(
                nearest, static_cast<std::uint32_t>(
                             std::abs(static_cast<std::int32_t>(center.x) - source.tile.x) +
                             std::abs(static_cast<std::int32_t>(center.y) - source.tile.y)));
        }
        all_distance += nearest;
        if (building.damage != aetheria::site::ProceduralBuildingDamage::Intact) {
            damaged_distance += nearest;
            ++damaged_count;
        }
    }
    ASSERT_GT(damaged_count, 0U);
    EXPECT_LT(damaged_distance * layer.buildings.size() * 100U, all_distance * damaged_count * 80U);
    std::cout << "site_damage_distance damaged_avg="
              << static_cast<double>(damaged_distance) / damaged_count
              << " all_avg=" << static_cast<double>(all_distance) / layer.buildings.size()
              << " damaged=" << damaged_count << '/' << layer.buildings.size()
              << " gates=" << layer.wall_gates.size() << " breaches=" << layer.wall_breaches.size()
              << '\n';
}

TEST(SiteFill, VillageAndMetropolisUseQuotasOnTheSamePath) {
    SiteFastVars village_fast{};
    village_fast.settlement = aetheria::world::SettlementTier::Village;
    village_fast.population = 250;
    village_fast.development_level = 1;
    auto metropolis_fast = village_fast;
    metropolis_fast.settlement = aetheria::world::SettlementTier::City;
    metropolis_fast.population = 8000;
    metropolis_fast.development_level = 20;

    const auto village = generate_fill(village_fast);
    const auto metropolis = generate_fill(metropolis_fast);
    const auto small = fill_counts(village);
    const auto large = fill_counts(metropolis);
    EXPECT_EQ(small.residential_blocks, 1U);
    EXPECT_EQ(small.commercial_blocks, 1U);
    EXPECT_GT(large.residential_blocks, small.residential_blocks);
    EXPECT_GT(large.commercial_blocks, small.commercial_blocks);
    EXPECT_GT(large.residential_buildings, small.residential_buildings);
    EXPECT_GT(large.commercial_buildings, small.commercial_buildings);
    std::cout << "site_fill_village residential_blocks=" << small.residential_blocks
              << " commercial_blocks=" << small.commercial_blocks
              << " residential_buildings=" << small.residential_buildings
              << " commercial_buildings=" << small.commercial_buildings << '\n'
              << "site_fill_metropolis residential_blocks=" << large.residential_blocks
              << " commercial_blocks=" << large.commercial_blocks
              << " residential_buildings=" << large.residential_buildings
              << " commercial_buildings=" << large.commercial_buildings << '\n';
}

TEST(SiteFill, BuildingsAreDisjointBuildableStreetFrontageWithCourtyards) {
    SiteFastVars fast{};
    fast.settlement = aetheria::world::SettlementTier::City;
    fast.population = 8000;
    fast.development_level = 20;
    const auto layer = generate_fill(fast);
    std::vector<std::uint8_t> occupied(aetheria::site::kSiteTileCount);
    std::size_t occupied_area{};
    std::size_t zoned_area{};
    for (std::size_t index = 0; index < layer.zoning.size(); ++index) {
        zoned_area += layer.zoning[index] != SiteZoning::Open ? 1U : 0U;
    }
    for (const auto& building : layer.buildings) {
        EXPECT_TRUE(frontage_is_road(layer, building));
        for (std::uint16_t y = building.origin.y; y < building.origin.y + building.height; ++y) {
            for (std::uint16_t x = building.origin.x; x < building.origin.x + building.width; ++x) {
                const auto index = fill_tile_index(x, y);
                EXPECT_EQ(occupied[index], 0U);
                EXPECT_NE(layer.zoning[index], SiteZoning::Open);
                EXPECT_NE(layer.skeleton.buildable[index], 0U);
                occupied[index] = UINT8_C(1);
                ++occupied_area;
            }
        }
    }
    EXPECT_TRUE(layer.valid_layout());
    EXPECT_GT(layer.buildings.size(), 0U);
    EXPECT_LT(occupied_area, zoned_area);
}

TEST(SiteFill, SkeletonAndFillFitThirtyMillisecondBudget) {
    SiteFastVars fast{};
    fast.owner = static_cast<aetheria::world::FactionId>(1);
    fast.settlement = aetheria::world::SettlementTier::City;
    fast.population = 100000;
    fast.development_level = 100;
    fast.defense = 100;
    fast.damage = 100;
    constexpr auto seed = UINT64_C(0xF1131);
    static_cast<void>(aetheria::site::populate(
        aetheria::site::build_site_skeleton(fill_slow_vars(true), seed, test_ruleset()), fast,
        test_ruleset()));
    const auto start = std::chrono::steady_clock::now();
    const auto result = aetheria::site::populate(
        aetheria::site::build_site_skeleton(fill_slow_vars(true), seed, test_ruleset()), fast,
        test_ruleset());
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const auto milliseconds = std::chrono::duration<double, std::milli>{elapsed}.count();
#ifdef NDEBUG
    constexpr auto kind = "Release";
#else
    constexpr auto kind = "Debug";
#endif
    EXPECT_TRUE(result.valid_layout());
    EXPECT_LT(elapsed, std::chrono::milliseconds{30});
    std::cout << "site_skeleton_fill_" << kind << "_ms=" << milliseconds << '\n';
}

}  // namespace

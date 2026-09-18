#include "tests/site/site_fill_test_support.h"
#include "tests/support/performance.h"

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

}  // namespace

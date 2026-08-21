#include "core/site/site_projection.h"
#include "tests/support/ruleset_fixture.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <gtest/gtest.h>

namespace {

using aetheria::site::SiteBoundarySide;
using aetheria::site::SiteSkeleton;
using aetheria::site::SiteSlowVars;
using aetheria::tests::test_ruleset;

[[nodiscard]] SiteSlowVars sample_slow_vars() {
  const auto &ruleset = test_ruleset();
  return {*ruleset.find_terrain("terrain.grassland"),
          *ruleset.find_relief("relief.plain"),
          *ruleset.find_feature("feature.none"),
          3200,
          {*ruleset.find_edge("edge.stream"), *ruleset.find_edge("edge.road"),
           *ruleset.find_edge("edge.river"), *ruleset.find_edge("edge.none")}};
}

[[nodiscard]] std::array<bool, 4> road_sides(const SiteSlowVars &slow) {
  std::array<bool, 4> result{};
  for (std::size_t index = 0; index < result.size(); ++index) {
    result[index] = (test_ruleset().edge(slow.edges[index])->flags &
                     aetheria::rules::kEdgeRoadFlag) != 0;
  }
  return result;
}

[[nodiscard]] std::array<bool, 4> gate_sides(const SiteSkeleton &skeleton) {
  std::array<bool, 4> result{};
  for (const auto &gate : skeleton.gates) {
    result[static_cast<std::size_t>(gate.side)] = true;
    switch (gate.side) {
    case SiteBoundarySide::North:
      EXPECT_EQ(gate.tile.y, 0U);
      break;
    case SiteBoundarySide::East:
      EXPECT_EQ(gate.tile.x, aetheria::site::kSiteWidth - 1U);
      break;
    case SiteBoundarySide::South:
      EXPECT_EQ(gate.tile.y, aetheria::site::kSiteHeight - 1U);
      break;
    case SiteBoundarySide::West:
      EXPECT_EQ(gate.tile.x, 0U);
      break;
    }
  }
  return result;
}

TEST(SiteSkeleton, TerrainHeightAndBoundaryWaterHaveRealContent) {
  const auto skeleton = aetheria::site::build_site_skeleton(
      sample_slow_vars(), UINT64_C(0x3015), test_ruleset());
  ASSERT_TRUE(skeleton.valid_layout());
  const auto [minimum, maximum] =
      std::ranges::minmax_element(skeleton.elevation);
  EXPECT_LT(*minimum, *maximum);
  EXPECT_TRUE(std::ranges::any_of(skeleton.water,
                                  [](auto value) { return value != 0; }));
  EXPECT_TRUE(std::ranges::any_of(skeleton.buildable,
                                  [](auto value) { return value != 0; }));
}

TEST(SiteSkeleton, GateSidesExactlyMatchRegionRoadSides) {
  auto slow = sample_slow_vars();
  slow.edges = {*test_ruleset().find_edge("edge.highway"),
                *test_ruleset().find_edge("edge.stream"),
                *test_ruleset().find_edge("edge.none"),
                *test_ruleset().find_edge("edge.trail")};
  const auto skeleton = aetheria::site::build_site_skeleton(
      slow, UINT64_C(0x6A7E), test_ruleset());
  EXPECT_EQ(gate_sides(skeleton), road_sides(slow));
  ASSERT_EQ(skeleton.gates.size(), 2U);
  std::cout << "site_gate_example region_roads=N,W gates=N@("
            << skeleton.gates[0].tile.x << ',' << skeleton.gates[0].tile.y
            << "),W@(" << skeleton.gates[1].tile.x << ','
            << skeleton.gates[1].tile.y << ")\n";
}

TEST(SiteSkeleton, RoadlessRegionCreatesNoInventedGate) {
  auto slow = sample_slow_vars();
  std::ranges::fill(slow.edges, *test_ruleset().find_edge("edge.none"));
  const auto skeleton = aetheria::site::build_site_skeleton(
      slow, UINT64_C(0x1501A7ED), test_ruleset());
  EXPECT_TRUE(skeleton.gates.empty());
  EXPECT_EQ(gate_sides(skeleton), road_sides(slow));
}

TEST(SiteSkeleton, RecursiveBlocksAreOffCenterAndDataBounded) {
  const auto skeleton = aetheria::site::build_site_skeleton(
      sample_slow_vars(), UINT64_C(0xB10C5), test_ruleset());
  ASSERT_GE(skeleton.blocks.size(), 30U);
  ASSERT_LE(skeleton.blocks.size(), 60U);
  std::vector<std::uint32_t> areas;
  areas.reserve(skeleton.blocks.size());
  for (const auto &block : skeleton.blocks) {
    areas.push_back(block.area());
  }
  std::ranges::sort(areas);
  const auto minimum = areas.front();
  const auto median = areas[areas.size() / 2U];
  const auto maximum = areas.back();
  EXPECT_LT(minimum, median);
  EXPECT_LT(median, maximum);
  std::cout << "site_blocks count=" << areas.size() << " area_min=" << minimum
            << " area_median=" << median << " area_max=" << maximum << '\n';
}

TEST(SiteSkeleton, BuildableMaskExcludesWaterRoadsAndSteepSlopes) {
  auto slow = sample_slow_vars();
  slow.relief = *test_ruleset().find_relief("relief.mountain");
  const auto skeleton = aetheria::site::build_site_skeleton(
      slow, UINT64_C(0xB017D), test_ruleset());
  std::size_t water_count{};
  std::size_t road_count{};
  std::size_t steep_count{};
  const auto max_slope =
      test_ruleset().site_generation_rules().max_buildable_slope;
  for (std::uint16_t y = 0; y < aetheria::site::kSiteHeight; ++y) {
    for (std::uint16_t x = 0; x < aetheria::site::kSiteWidth; ++x) {
      const aetheria::site::SiteXY tile{x, y};
      const auto index =
          static_cast<std::size_t>(y) * aetheria::site::kSiteWidth + x;
      if (skeleton.water[index] != 0) {
        ++water_count;
        EXPECT_FALSE(skeleton.is_buildable(tile));
      }
      if (skeleton.roads[index] != 0) {
        ++road_count;
        EXPECT_FALSE(skeleton.is_buildable(tile));
      }
      const auto center = skeleton.elevation[index];
      if (x + 1U < aetheria::site::kSiteWidth) {
        const auto east = skeleton.elevation[index + 1U];
        const auto slope = static_cast<std::uint16_t>(
            std::abs(static_cast<std::int32_t>(center) - east));
        if (slope > max_slope) {
          ++steep_count;
          EXPECT_FALSE(skeleton.is_buildable(tile));
          EXPECT_FALSE(
              skeleton.is_buildable({static_cast<std::uint16_t>(x + 1U), y}));
        }
      }
    }
  }
  EXPECT_GT(water_count, 0U);
  EXPECT_GT(road_count, 0U);
  EXPECT_GT(steep_count, 0U);
}

TEST(SiteSkeleton, FitsThirtyMillisecondBudget) {
  auto slow = sample_slow_vars();
  std::ranges::fill(slow.edges, *test_ruleset().find_edge("edge.road"));
  static_cast<void>(
      aetheria::site::build_site_skeleton(slow, UINT64_C(1), test_ruleset()));
  const auto start = std::chrono::steady_clock::now();
  const auto skeleton =
      aetheria::site::build_site_skeleton(slow, UINT64_C(2), test_ruleset());
  const auto elapsed = std::chrono::steady_clock::now() - start;
  const auto milliseconds =
      std::chrono::duration<double, std::milli>{elapsed}.count();
  EXPECT_TRUE(skeleton.valid_layout());
  EXPECT_LT(elapsed, std::chrono::milliseconds{30});
#ifdef NDEBUG
  std::cout << "site_skeleton_Release_ms=" << milliseconds << '\n';
#else
  std::cout << "site_skeleton_Debug_ms=" << milliseconds << '\n';
#endif
}

} // namespace
